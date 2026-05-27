/**
 * Day 14: ZK-style WAL + manifest pointer flip (ATOM-XIV).
 */

#include "bs/adapter/persistence/attach_store.h"
#include "bs/adapter/persistence/attach_wal.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include <filesystem>

#include "attach_crc32.h"

namespace fs = std::filesystem;

static void write_legacy_manifest(const fs::path& manifest, const std::string& uri,
                                  uint64_t rev, const char* path)
{
    std::ofstream out(manifest, std::ios::trunc);
    out << "# BlessStar manifest v1\nbatch_epoch=1\n";
    out << "[uri=" << uri << "]\nrevision=" << rev << "\n";
    if (path)
        out << "path=" << path << "\n";
}

static uint64_t read_epoch(const fs::path& manifest)
{
    BsAttachStore* s = bs_attach_store_open(manifest.string().c_str());
    assert(s != nullptr);
    const uint64_t e = bs_attach_store_batch_epoch(s);
    bs_attach_store_close(s);
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

    BsAttachStore* store = bs_attach_store_open(man.string().c_str());
    assert(store != nullptr);
    bs_attach_store_batch_begin(store);
    assert(bs_attach_store_batch_stage(store, uri.c_str(), data, len, 0) == BS_ATTACH_OK);
    assert(bs_attach_store_batch_commit(store) == BS_ATTACH_OK);
    assert(bs_attach_store_batch_epoch(store) == 2);
    bs_attach_store_close(store);

    assert(fs::exists(wal));
    assert(read_epoch(man) == 2);

    {
        char path_buf[1024];
        BsAttachStore* rd = bs_attach_store_open(man.string().c_str());
        assert(rd != nullptr);
        assert(bs_attach_store_get_canonical_path(rd, uri.c_str(), path_buf, sizeof(path_buf)) ==
               BS_ATTACH_OK);
        assert(fs::exists(path_buf));
        bs_attach_store_close(rd);
    }

    {
        std::ifstream in(man);
        std::string   content((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
        assert(content.find("manifest_checksum=") != std::string::npos);
        assert(fs::exists(man.string() + ".prev"));
    }

    {
        std::ofstream corrupt(man, std::ios::trunc);
        corrupt << "# corrupt\nmanifest_checksum=deadbeef\nbatch_epoch=2\n";
        corrupt.close();
        BsAttachStore* rd = bs_attach_store_open(man.string().c_str());
        assert(rd != nullptr);
        assert(bs_attach_store_batch_epoch(rd) >= 0);
        bs_attach_store_close(rd);
        assert(fs::exists(man));
    }

    {
        const std::string orphan_path = (tmp / "orphan.staging").string();
        BsAttachWalEntry  e{};
        e.uri               = uri.c_str();
        e.staging_path      = orphan_path.c_str();
        e.expected_rev        = 0;
        e.new_rev             = 1;
        e.payload_checksum    = bs_attach_crc32(data, len);
        {
            std::ofstream orphan(tmp / "orphan.staging");
            orphan << "orphan";
        }
        BsAttachWal* w = bs_attach_wal_open(wal.string().c_str());
        assert(w != nullptr);
        assert(bs_attach_wal_append_batch(w, 99, &e, 1) == BS_ATTACH_OK);
        bs_attach_wal_close(w);
        assert(fs::exists(tmp / "orphan.staging"));
        BsAttachStore* rd = bs_attach_store_open(man.string().c_str());
        assert(rd != nullptr);
        bs_attach_store_close(rd);
        assert(!fs::exists(tmp / "orphan.staging"));
    }

    fs::remove_all(tmp);
    return 0;
}
