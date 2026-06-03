/**
 * Day 14: ZK-style WAL + manifest pointer flip (ATOM-XIV).
 */

#include "bs/adapter/persistence/attach_store.h"
#include "bs/adapter/persistence/attach_wal.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "attach_crc32.h"

namespace fs = std::filesystem;

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
    c ^= 0x5A;
    f.write(&c, 1);
    f.flush();
}

static void write_corrupt_len_record(const fs::path& wal_path)
{
    std::ofstream out(wal_path, std::ios::binary | std::ios::trunc);
    assert(out);
    auto w16 = [&](uint16_t v)
    {
        char b[2] = {(char)(v & 0xFFu), (char)((v >> 8) & 0xFFu)};
        out.write(b, 2);
    };
    auto w32 = [&](uint32_t v)
    {
        char b[4] = {(char)(v & 0xFFu), (char)((v >> 8) & 0xFFu), (char)((v >> 16) & 0xFFu),
                     (char)((v >> 24) & 0xFFu)};
        out.write(b, 4);
    };
    const uint32_t magic = 0x42535741u; // same as attach_wal.c
    w32(magic);
    w16(1);
    w16(1);
    w32(BS_ATTACH_WAL_MAX_RECORD_BYTES + 1);
    w32(0); // crc (ignored because header should fail len cap)
    out.flush();
}

static void write_legacy_manifest(const fs::path& manifest, const std::string& uri, uint64_t rev,
                                  const char* path)
{
    std::ofstream out(manifest, std::ios::trunc);
    out << "# BlessStar manifest v1\nbatch_epoch=1\n";
    out << "[uri=" << uri << "]\nrevision=" << rev << "\n";
    if (path)
        out << "path=" << path << "\n";
}

static uint64_t read_epoch(const fs::path& manifest)
{
    BsAttachStore* s = bs_adapter_attach_persist_store_open(manifest.string().c_str());
    assert(s != nullptr);
    const uint64_t e = bs_adapter_attach_persist_store_batch_epoch(s);
    bs_adapter_attach_persist_store_close(s);
    return e;
}

int main()
{
    const fs::path tmp = fs::temp_directory_path() / "bs_day14_atomic";
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

    BsAttachStore* store = bs_adapter_attach_persist_store_open(man.string().c_str());
    assert(store != nullptr);
    bs_adapter_attach_persist_store_batch_begin(store);
    assert(bs_adapter_attach_persist_store_batch_stage(store, uri.c_str(), data, len, 0) ==
           BS_ATTACH_OK);
    assert(bs_adapter_attach_persist_store_batch_commit(store) == BS_ATTACH_OK);
    assert(bs_adapter_attach_persist_store_batch_epoch(store) == 2);
    bs_adapter_attach_persist_store_close(store);

    assert(fs::exists(wal.string() + ".e2"));
    assert(read_epoch(man) == 2);

    {
        char           path_buf[1024];
        BsAttachStore* rd = bs_adapter_attach_persist_store_open(man.string().c_str());
        assert(rd != nullptr);
        assert(bs_adapter_attach_persist_store_get_canonical_path(
                   rd, uri.c_str(), path_buf, sizeof(path_buf)) == BS_ATTACH_OK);
        assert(fs::exists(path_buf));
        bs_adapter_attach_persist_store_close(rd);
    }

    {
        std::ifstream in(man);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content.find("manifest_checksum=") != std::string::npos);
        assert(fs::exists(man.string() + ".prev"));
    }

    {
        std::ofstream corrupt(man, std::ios::trunc);
        corrupt << "# corrupt\nmanifest_checksum=deadbeef\nbatch_epoch=2\n";
        corrupt.close();
        BsAttachStore* rd = bs_adapter_attach_persist_store_open(man.string().c_str());
        assert(rd != nullptr);
        assert(bs_adapter_attach_persist_store_batch_epoch(rd) >= 0);
        bs_adapter_attach_persist_store_close(rd);
        assert(fs::exists(man));
    }

    {
        const std::string orphan_path = (tmp / "orphan.staging").string();
        BsAttachWalEntry  e{};
        e.uri              = uri.c_str();
        e.staging_path     = orphan_path.c_str();
        e.expected_rev     = 0;
        e.new_rev          = 1;
        e.payload_checksum = bs_adapter_attach_persist_crc32(data, len);
        {
            std::ofstream orphan(tmp / "orphan.staging");
            orphan << "orphan";
        }
        BsAttachWal* w = bs_adapter_attach_persist_wal_open(wal.string().c_str());
        assert(w != nullptr);
        assert(bs_adapter_attach_persist_wal_append_batch(w, 99, &e, 1) == BS_ATTACH_OK);
        bs_adapter_attach_persist_wal_close(w);
        assert(fs::exists(tmp / "orphan.staging"));
        BsAttachStore* rd = bs_adapter_attach_persist_store_open(man.string().c_str());
        assert(rd != nullptr);
        bs_adapter_attach_persist_store_close(rd);
        assert(!fs::exists(tmp / "orphan.staging"));
    }

    {
        const fs::path orphan2 = tmp / "orphan_corrupt_crc.staging";
        {
            std::ofstream orphan(orphan2);
            orphan << "orphan2";
        }
        BsAttachWalEntry e{};
        e.uri              = uri.c_str();
        e.staging_path     = orphan2.string().c_str();
        e.expected_rev     = 0;
        e.new_rev          = 1;
        e.payload_checksum = bs_adapter_attach_persist_crc32(data, len);

        BsAttachWal* w = bs_adapter_attach_persist_wal_open(wal.string().c_str());
        assert(w != nullptr);
        assert(bs_adapter_attach_persist_wal_append_batch(w, 100, &e, 1) == BS_ATTACH_OK);
        bs_adapter_attach_persist_wal_close(w);

        flip_last_byte(wal);
        assert(fs::exists(orphan2));
        BsAttachStore* rd = bs_adapter_attach_persist_store_open(man.string().c_str());
        assert(rd != nullptr);
        bs_adapter_attach_persist_store_close(rd);
        // ATOM-REC-SAFE-2: corruption => conservative (no deletions).
        assert(fs::exists(orphan2));
    }

    {
        const fs::path orphan3 = tmp / "orphan_corrupt_len.staging";
        {
            std::ofstream orphan(orphan3);
            orphan << "orphan3";
        }
        write_corrupt_len_record(wal);
        BsAttachStore* rd = bs_adapter_attach_persist_store_open(man.string().c_str());
        assert(rd != nullptr);
        bs_adapter_attach_persist_store_close(rd);
        assert(fs::exists(orphan3));
    }

    fs::remove_all(tmp);
    return 0;
}
