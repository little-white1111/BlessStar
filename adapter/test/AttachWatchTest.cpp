/**
 * Day 15: scheme B + watch bus smoke.
 */

#include "bs/adapter/persistence/attach_store.h"
#include "bs/adapter/persistence/attach_wal.h"
#include "bs/adapter/persistence/attach_watch.h"

#include <cassert>
#include <chrono>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static void write_legacy_manifest(const fs::path& manifest, const std::string& uri, uint64_t rev,
                                  const char* path)
{
    std::ofstream out(manifest, std::ios::trunc);
    out << "# BlessStar manifest v1\nbatch_epoch=1\n";
    out << "[uri=" << uri << "]\nrevision=" << rev << "\n";
    if (path)
        out << "path=" << path << "\n";
}

static int always_fail_subscriber(const BsAttachWatchEvent* ev, void* user)
{
    (void)ev;
    (void)user;
    return -7;
}

static void flip_last_byte(const fs::path& p)
{
    std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
    assert(f);
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    assert(n > 0);
    f.seekg(n - 1);
    char c = 0;
    f.read(&c, 1);
    f.seekp(n - 1);
    c ^= 0x35;
    f.write(&c, 1);
    f.flush();
}

int main()
{
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
    const uint32_t     salt = std::random_device{}();
    std::ostringstream dir_name;
    dir_name << "bs_day15_watch_" << now_ns << "_" << salt;
    const fs::path tmp = fs::temp_directory_path() / dir_name.str();
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    const fs::path cfg  = tmp / "cfg.json";
    const fs::path man  = tmp / "manifest.bs";
    const fs::path wal  = tmp / "manifest.bs.wal";
    const char*    data = "{\"k\":1}";
    const size_t   len  = std::strlen(data);
    {
        std::ofstream out(cfg, std::ios::binary);
        out.write(data, static_cast<std::streamsize>(len));
    }

    std::string uri = "file:///" + fs::absolute(cfg).string();
    for (char& c : uri)
    {
        if (c == '\\')
            c = '/';
    }
    write_legacy_manifest(man, uri, 0, cfg.string().c_str());

    bs_adapter_attach_persist_watch_metrics_reset();
    bs_adapter_attach_persist_watch_audit_reset();
    int tok_metrics = 0;
    int tok_audit   = 0;
    int tok_fail    = 0;
    assert(bs_adapter_attach_persist_watch_subscribe(
               bs_adapter_attach_persist_watch_metrics_on_event, nullptr, &tok_metrics) == 0);
    assert(bs_adapter_attach_persist_watch_subscribe(bs_adapter_attach_persist_watch_audit_on_event,
                                                     nullptr, &tok_audit) == 0);
    assert(bs_adapter_attach_persist_watch_subscribe(always_fail_subscriber, nullptr, &tok_fail) ==
           0);

    BsAttachStore* store = bs_adapter_attach_persist_store_open(man.string().c_str());
    assert(store != nullptr);
    bs_adapter_attach_persist_store_batch_begin(store);
    assert(bs_adapter_attach_persist_store_batch_stage(store, uri.c_str(), data, len, 0) ==
           BS_ATTACH_OK);
    // WATCH-XV-4: subscriber failure must not block commit.
    assert(bs_adapter_attach_persist_store_batch_commit(store) == BS_ATTACH_OK);
    bs_adapter_attach_persist_store_close(store);

    BsAttachWatchMetrics m{};
    bs_adapter_attach_persist_watch_metrics_snapshot(&m);
    assert(m.total_events > 0);
    assert(m.stage_counts[BS_ATTACH_WATCH_STAGE_CAS] >= 1);
    assert(m.stage_counts[BS_ATTACH_WATCH_STAGE_WAL_FSYNC] >= 1);
    assert(m.dedupe_capacity == bs_adapter_attach_persist_watch_dedupe_capacity());

    // Corrupt active wal -> recover emits RECOVER_CONSERVATIVE.
    const std::string orphan = (tmp / "orphan_watch.staging").string();
    BsAttachWalEntry  e{};
    e.uri              = uri.c_str();
    e.staging_path     = orphan.c_str();
    e.expected_rev     = 0;
    e.new_rev          = 1;
    e.payload_checksum = 1;
    BsAttachWal* w     = bs_adapter_attach_persist_wal_open(wal.string().c_str());
    assert(w != nullptr);
    assert(bs_adapter_attach_persist_wal_append_batch(w, 100, &e, 1) == BS_ATTACH_OK);
    bs_adapter_attach_persist_wal_close(w);
    flip_last_byte(wal);

    BsAttachStore* rd = bs_adapter_attach_persist_store_open(man.string().c_str());
    assert(rd != nullptr);
    bs_adapter_attach_persist_store_close(rd);

    BsAttachWatchAudit a{};
    bs_adapter_attach_persist_watch_audit_snapshot(&a);
    assert(a.conservative_recover_count >= 1);
    assert(a.publish_callback_error_count >= 1);
    assert(a.last_callback_error_stage >= BS_ATTACH_WATCH_STAGE_CAS);

    bs_adapter_attach_persist_watch_unsubscribe(tok_fail);
    bs_adapter_attach_persist_watch_unsubscribe(tok_audit);
    bs_adapter_attach_persist_watch_unsubscribe(tok_metrics);

    fs::remove_all(tmp);
    return 0;
}
