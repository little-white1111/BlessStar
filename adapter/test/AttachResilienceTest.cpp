/**
 * Day 12: attach persistence, CAS, per_batch atomicity, OOM hooks (RES-IX).
 */

#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"

#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/adapter/persistence/attach_store.h"
#include "bs/kernel/report/report.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static void noop_log(uint16_t, BsLogLevel, const char*, void*) {}

static int golden_read(void*, const char*, IoReadResult* out)
{
    bs_io_read_result_init(out);
    out->status = BS_IO_OK;
    out->length = kBlessStarConfigV1GoldenLen;
    out->data   = static_cast<uint8_t*>(std::malloc(out->length));
    if (!out->data)
        return BS_IO_ERR_PROVIDER;
    std::memcpy(out->data, kBlessStarConfigV1Golden, out->length);
    return BS_IO_OK;
}

static int io_oom_read(void*, const char*, IoReadResult* out)
{
    bs_io_read_result_init(out);
    out->status        = BS_IO_ERR_PROVIDER;
    out->error_message = static_cast<char*>(std::malloc(4));
    if (out->error_message)
        std::strcpy(out->error_message, "oom");
    return BS_IO_ERR_PROVIDER;
}

static int reject_bad_uri_gate(void*, const char* uri, const IoReadResult* read_result,
                               BsReloadGateDetail* detail_out)
{
    (void)detail_out;
    if (!read_result || read_result->status != BS_IO_OK)
        return BS_RELOAD_GATE_PARSE_FAIL;
    if (std::strstr(uri, "bad") != nullptr)
        return BS_RELOAD_GATE_IR_REJECT;
    return BS_RELOAD_GATE_OK;
}

static std::string file_uri_for(const fs::path& p)
{
    std::string u = "file:///" + fs::absolute(p).string();
    for (char& c : u)
    {
        if (c == '\\')
            c = '/';
    }
    return u;
}

static void write_manifest_revision(const fs::path& manifest, const std::string& uri,
                                  uint64_t rev, const char* canonical_path = nullptr)
{
    std::ofstream out(manifest, std::ios::trunc);
    out << "# test manifest\nbatch_epoch=1\n";
    out << "[uri=" << uri << "]\nrevision=" << rev << "\n";
    if (canonical_path)
        out << "path=" << canonical_path << "\n";
}

static void write_manifest_two(const fs::path& manifest, const std::string& uri_a,
                               const std::string& uri_b, uint64_t rev)
{
    std::ofstream out(manifest, std::ios::trunc);
    out << "# test manifest\nbatch_epoch=0\n";
    out << "[uri=" << uri_a << "]\nrevision=" << rev << "\n";
    out << "[uri=" << uri_b << "]\nrevision=" << rev << "\n";
}

static uint64_t read_manifest_revision(const fs::path& manifest, const std::string& uri)
{
    BsAttachStore* s = bs_attach_store_open(manifest.string().c_str());
    assert(s != nullptr);
    uint64_t rev = 0;
    assert(bs_attach_store_get_revision(s, uri.c_str(), &rev) == BS_ATTACH_OK);
    bs_attach_store_close(s);
    return rev;
}

#if defined(BS_TESTING)
static int g_malloc_fail_after = -1;
static int g_malloc_calls      = 0;

static void* counting_malloc_fail(size_t n)
{
    ++g_malloc_calls;
    if (g_malloc_fail_after >= 0 && g_malloc_calls > g_malloc_fail_after)
        return nullptr;
    return std::malloc(n);
}
#endif

int main()
{
    assert(bs_adapter_log_bind_memory_bus(noop_log, nullptr) == 0);

    ReloadBatchController* ctrl = bs_reload_batch_controller_create(4);
    assert(ctrl != nullptr);
    bs_reload_batch_controller_set_read_fn(ctrl, golden_read, nullptr);
    bs_reload_batch_controller_use_default_gate(ctrl);
    assert(bs_reload_batch_add_path(ctrl, "file:///x") == 0);
    assert(bs_reload_batch_run(ctrl) == -4);
    bs_reload_batch_controller_destroy(ctrl);

    const fs::path tmp = fs::temp_directory_path() / "bs_day12_attach";
    fs::create_directories(tmp);
    const fs::path cfg  = tmp / "cfg.json";
    const fs::path man  = tmp / "manifest.bs";
    {
        std::ofstream out(cfg, std::ios::binary);
        out.write(kBlessStarConfigV1Golden,
                  static_cast<std::streamsize>(kBlessStarConfigV1GoldenLen));
    }
    const std::string uri = file_uri_for(cfg);
    write_manifest_revision(man, uri, 0, cfg.string().c_str());

    ctrl = bs_reload_batch_controller_create(4);
    bs_reload_batch_controller_set_read_fn(ctrl, golden_read, nullptr);
    bs_reload_batch_controller_use_default_gate(ctrl);
    bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_reload_batch_controller_set_manifest_path(ctrl, man.string().c_str());
    Report* report = report_create("attach_audit");
    assert(report != nullptr);
    bs_reload_batch_controller_set_report(ctrl, report);
    assert(bs_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    char* rjson = report_to_json(report);
    assert(rjson != nullptr);
    assert(std::strstr(rjson, "scheme=per_path") != nullptr);
    assert(std::strstr(rjson, "batch_epoch=") != nullptr);
    assert(std::strstr(rjson, "revision_base=") != nullptr);
    std::free(rjson);
    report_destroy(report);
    assert(read_manifest_revision(man, uri) == 1);
    {
        char path_buf[512];
        BsAttachStore* rd = bs_attach_store_open(man.string().c_str());
        assert(rd != nullptr);
        assert(bs_attach_store_get_canonical_path(rd, uri.c_str(), path_buf, sizeof(path_buf)) ==
               BS_ATTACH_OK);
        bs_attach_store_close(rd);
    }
    bs_reload_batch_controller_destroy(ctrl);

    write_manifest_revision(man, uri, 0);
    {
        BsAttachStore* s = bs_attach_store_open(man.string().c_str());
        assert(s != nullptr);
        assert(bs_attach_store_commit_per_path(s, uri.c_str(), kBlessStarConfigV1Golden,
                                               kBlessStarConfigV1GoldenLen, 0) == BS_ATTACH_OK);
        assert(bs_attach_store_commit_per_path(s, uri.c_str(), kBlessStarConfigV1Golden,
                                               kBlessStarConfigV1GoldenLen, 0) ==
               BS_ATTACH_ERR_CONFLICT);
        assert(read_manifest_revision(man, uri) == 1);
        bs_attach_store_close(s);
    }

    const fs::path cfg2 = tmp / "cfg2.json";
    {
        std::ofstream out(cfg2, std::ios::binary);
        out.write(kBlessStarConfigV1Golden,
                  static_cast<std::streamsize>(kBlessStarConfigV1GoldenLen));
    }
    const std::string uri2 = file_uri_for(cfg2);
    write_manifest_two(man, uri, uri2, 0);
    ctrl = bs_reload_batch_controller_create(8);
    bs_reload_batch_controller_set_read_fn(ctrl, golden_read, nullptr);
    bs_reload_batch_controller_use_default_gate(ctrl);
    bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_BATCH);
    bs_reload_batch_controller_set_manifest_path(ctrl, man.string().c_str());
    assert(bs_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    assert(bs_reload_batch_add_path(ctrl, uri2.c_str()) == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    assert(read_manifest_revision(man, uri) == 1);
    assert(read_manifest_revision(man, uri2) == 1);
    bs_reload_batch_controller_destroy(ctrl);

    write_manifest_two(man, uri, uri2, 0);
    ctrl = bs_reload_batch_controller_create(8);
    bs_reload_batch_controller_set_read_fn(ctrl, golden_read, nullptr);
    bs_reload_batch_controller_set_gate_fn(ctrl, reject_bad_uri_gate, nullptr);
    bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_BATCH);
    bs_reload_batch_controller_set_manifest_path(ctrl, man.string().c_str());
    const std::string bad_uri = file_uri_for(tmp / "bad.json");
    assert(bs_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    assert(bs_reload_batch_add_path(ctrl, bad_uri.c_str()) == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);
    assert(read_manifest_revision(man, uri) == 0);
    assert(read_manifest_revision(man, uri2) == 0);
    bs_reload_batch_controller_destroy(ctrl);

    write_manifest_revision(man, uri, 0);
    ctrl = bs_reload_batch_controller_create(4);
    bs_reload_batch_controller_set_read_fn(ctrl, io_oom_read, nullptr);
    bs_reload_batch_controller_use_default_gate(ctrl);
    bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_reload_batch_controller_set_manifest_path(ctrl, man.string().c_str());
    assert(bs_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);
    assert(read_manifest_revision(man, uri) == 0);
    bs_reload_batch_controller_destroy(ctrl);

#if defined(BS_TESTING)
    bs_attach_store_set_malloc_hook(counting_malloc_fail);
    g_malloc_calls      = 0;
    g_malloc_fail_after = 0;
    assert(bs_attach_store_open(nullptr) == nullptr);
    bs_attach_store_reset_malloc_hook();
#endif

    ctrl = bs_reload_batch_controller_create(4);
    bs_reload_batch_controller_set_read_fn(ctrl, golden_read, nullptr);
    bs_reload_batch_controller_use_default_gate(ctrl);
    day12_wire_reload_defaults(ctrl);
    bs_reload_batch_controller_set_session_memory_cap(ctrl, 16);
    assert(bs_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);
    bs_reload_batch_controller_destroy(ctrl);

    fs::remove_all(tmp);
    return 0;
}
