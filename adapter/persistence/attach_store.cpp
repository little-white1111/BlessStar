#include "bs/adapter/persistence/attach_store.h"
#include "bs/adapter/persistence/attach_wal.h"
#include "bs/adapter/persistence/attach_watch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "attach_crc32.h"
#include "attach_fsync.h"
#include "attach_uri_path.h"

struct StagedEntry
{
    std::string          uri;
    std::string          path;
    std::vector<uint8_t> data;
    uint64_t             expected_rev = 0;
};

struct BsAttachStore
{
    std::string                                  manifest_path;
    std::string                                  wal_path;
    bool                                         memory_only = false;
    uint64_t                                     batch_epoch = 0;
    std::unordered_map<std::string, uint64_t>    revisions;
    std::unordered_map<std::string, std::string> canonical_paths;
    std::vector<StagedEntry>                     staged;
    bool                                         batch_open   = false;
    BsAttachWal*                                 wal          = nullptr;
    BsAttachFsyncPolicy                          fsync_policy = BS_ATTACH_FSYNC_BATCH_COMMIT;
};

static BsAttachMallocFn g_malloc_hook = nullptr;

static void* attach_malloc(size_t n)
{
    if (g_malloc_hook)
        return g_malloc_hook(n);
    return std::malloc(n);
}

extern "C" void bs_adapter_attach_persist_store_set_malloc_hook(BsAttachMallocFn fn)
{
    g_malloc_hook = fn;
}

extern "C" void bs_adapter_attach_persist_store_reset_malloc_hook(void)
{
    g_malloc_hook = nullptr;
}

void bs_adapter_attach_persist_store_set_fsync_policy(BsAttachStore*      store,
                                                      BsAttachFsyncPolicy policy)
{
    if (store)
        store->fsync_policy = policy;
}

BsAttachFsyncPolicy bs_adapter_attach_persist_store_get_fsync_policy(const BsAttachStore* store)
{
    return store ? store->fsync_policy : BS_ATTACH_FSYNC_BATCH_COMMIT;
}

static bool should_fsync_canonical(const BsAttachStore* store)
{
    return store && (store->fsync_policy == BS_ATTACH_FSYNC_ALWAYS);
}

static bool should_fsync_manifest(const BsAttachStore* store)
{
    return store && store->fsync_policy != BS_ATTACH_FSYNC_NEVER;
}

static std::string derive_wal_path(const std::string& manifest_path)
{
    return manifest_path + ".wal";
}

static std::string make_staging_path(const std::string& base_path, uint64_t epoch, uint64_t new_rev)
{
    return base_path + ".bs.staging.e" + std::to_string(epoch) + ".r" + std::to_string(new_rev);
}

static uint32_t crc32_payload(const void* data, size_t len)
{
    return bs_adapter_attach_persist_crc32(data, len);
}

static uint32_t crc32_manifest_body(const std::string& body)
{
    return bs_adapter_attach_persist_crc32(body.data(), body.size());
}

static void publish_watch_event(uint64_t epoch, const char* uri, BsAttachWatchStage stage,
                                BsAttachWatchResult result)
{
    BsAttachWatchEvent ev{};
    ev.epoch  = epoch;
    ev.uri    = uri ? uri : "";
    ev.stage  = stage;
    ev.result = result;
    (void)bs_adapter_attach_persist_watch_publish(
        &ev); // WATCH-XV-4: publish failure never blocks commit.
}

static int write_file_atomic_ex(const char* path, const void* data, size_t len, bool fsync_tmp)
{
    if (!path)
        return BS_ATTACH_ERR_INVALID_ARG;
    const std::string tmp = std::string(path) + ".bs.tmp";
    FILE*             f   = fopen(tmp.c_str(), "wb");
    if (!f)
        return BS_ATTACH_ERR_IO;
    if (len > 0)
    {
        if (fwrite(data, 1, len, f) != len)
        {
            fclose(f);
            std::remove(tmp.c_str());
            return BS_ATTACH_ERR_IO;
        }
    }
    if (fsync_tmp && bs_adapter_attach_persist_fsync_file(f) != 0)
    {
        fclose(f);
        std::remove(tmp.c_str());
        return BS_ATTACH_ERR_IO;
    }
    fclose(f);
    (void)std::remove(path);
    if (std::rename(tmp.c_str(), path) != 0)
    {
        std::remove(tmp.c_str());
        return BS_ATTACH_ERR_IO;
    }
    return BS_ATTACH_OK;
}

static int write_file_atomic(const char* path, const void* data, size_t len, bool fsync_tmp)
{
    return write_file_atomic_ex(path, data, len, fsync_tmp);
}

static int copy_file_sync(const std::string& src, const std::string& dst)
{
    std::ifstream in(src, std::ios::binary);
    if (!in)
        return BS_ATTACH_ERR_IO;
    FILE* out = fopen(dst.c_str(), "wb");
    if (!out)
        return BS_ATTACH_ERR_IO;
    char buf[4096];
    while (in.good())
    {
        in.read(buf, sizeof(buf));
        const std::streamsize n = in.gcount();
        if (n > 0 && fwrite(buf, 1, (size_t)n, out) != (size_t)n)
        {
            fclose(out);
            return BS_ATTACH_ERR_IO;
        }
        if (!in)
            break;
    }
    if (bs_adapter_attach_persist_fsync_file(out) != 0)
    {
        fclose(out);
        return BS_ATTACH_ERR_IO;
    }
    fclose(out);
    return BS_ATTACH_OK;
}

static int load_manifest_from_path(BsAttachStore* store, const std::string& path,
                                   bool verify_checksum)
{
    std::ifstream in(path);
    if (!in)
        return BS_ATTACH_ERR_IO;

    store->revisions.clear();
    store->canonical_paths.clear();

    std::string line;
    std::string current_uri;
    uint32_t    file_checksum = 0;
    bool        have_checksum = false;
    std::string body_for_crc;

    while (std::getline(in, line))
    {
        if (line.size() > BS_ATTACH_MAX_MANIFEST_LINE)
            return BS_ATTACH_ERR_LIMIT;
        if (line.empty() || line[0] == '#')
            continue;

        if (line.rfind("manifest_checksum=", 0) == 0)
        {
            file_checksum = (uint32_t)std::strtoul(line.c_str() + 18, nullptr, 16);
            have_checksum = true;
            continue;
        }
        if (line.rfind("batch_epoch=", 0) == 0)
        {
            body_for_crc += line;
            body_for_crc += '\n';
            store->batch_epoch =
                static_cast<uint64_t>(std::strtoull(line.c_str() + 12, nullptr, 10));
            continue;
        }
        if (line.rfind("[uri=", 0) == 0 && line.size() > 6 && line.back() == ']')
        {
            body_for_crc += line;
            body_for_crc += '\n';
            current_uri = line.substr(5, line.size() - 6);
            continue;
        }
        if (!current_uri.empty() && line.rfind("revision=", 0) == 0)
        {
            body_for_crc += line;
            body_for_crc += '\n';
            store->revisions[current_uri] =
                static_cast<uint64_t>(std::strtoull(line.c_str() + 9, nullptr, 10));
            continue;
        }
        if (!current_uri.empty() && line.rfind("path=", 0) == 0)
        {
            body_for_crc += line;
            body_for_crc += '\n';
            store->canonical_paths[current_uri] = line.substr(5);
        }
    }

    if (verify_checksum && have_checksum)
    {
        const uint32_t calc = crc32_manifest_body(body_for_crc);
        if (calc != file_checksum)
            return BS_ATTACH_ERR_IO;
    }
    return BS_ATTACH_OK;
}

static int load_manifest_file(BsAttachStore* store)
{
    if (!store || store->memory_only)
        return BS_ATTACH_OK;

    if (!std::ifstream(store->manifest_path).good())
    {
        store->revisions.clear();
        store->canonical_paths.clear();
        store->batch_epoch = 0;
        return BS_ATTACH_OK;
    }

    const int rc = load_manifest_from_path(store, store->manifest_path, true);
    if (rc == BS_ATTACH_OK)
        return BS_ATTACH_OK;

    // If the manifest exists but violates hard limits, fail open() to satisfy AUD-IX
    // expectations (invalid persisted state must not be silently accepted).
    if (rc == BS_ATTACH_ERR_LIMIT)
        return rc;

    const std::string prev = store->manifest_path + ".prev";
    if (load_manifest_from_path(store, prev, false) != BS_ATTACH_OK)
    {
        store->revisions.clear();
        store->canonical_paths.clear();
        store->batch_epoch = 0;
        return rc;
    }
    (void)copy_file_sync(prev, store->manifest_path);
    return BS_ATTACH_OK;
}

static int save_manifest_file(BsAttachStore* store)
{
    if (!store || store->memory_only)
        return BS_ATTACH_OK;

    std::ostringstream body;
    body << "batch_epoch=" << store->batch_epoch << "\n";
    for (const auto& kv : store->revisions)
    {
        body << "[uri=" << kv.first << "]\n";
        body << "revision=" << kv.second << "\n";
        const auto pit = store->canonical_paths.find(kv.first);
        if (pit != store->canonical_paths.end())
            body << "path=" << pit->second << "\n";
    }
    const std::string body_s = body.str();
    const uint32_t    csum   = crc32_manifest_body(body_s);

    std::ostringstream out;
    out << "# BlessStar manifest v1\n";
    out << "manifest_checksum=" << std::hex << csum << std::dec << "\n";
    out << body_s;

    const std::string final_s = out.str();
    const std::string tmp     = store->manifest_path + ".bs.tmp";
    const std::string prev    = store->manifest_path + ".prev";

    if (std::ifstream(store->manifest_path).good())
    {
        if (copy_file_sync(store->manifest_path, prev) != BS_ATTACH_OK)
            return BS_ATTACH_ERR_IO;
    }

    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f)
        return BS_ATTACH_ERR_IO;
    if (fwrite(final_s.data(), 1, final_s.size(), f) != final_s.size())
    {
        fclose(f);
        std::remove(tmp.c_str());
        return BS_ATTACH_ERR_IO;
    }
    if (should_fsync_manifest(store) && bs_adapter_attach_persist_fsync_file(f) != 0)
    {
        fclose(f);
        std::remove(tmp.c_str());
        return BS_ATTACH_ERR_IO;
    }
    fclose(f);

    (void)std::remove(store->manifest_path.c_str());
    if (std::rename(tmp.c_str(), store->manifest_path.c_str()) != 0)
        return BS_ATTACH_ERR_IO;
    return BS_ATTACH_OK;
}

static void open_wal(BsAttachStore* store)
{
    if (!store || store->memory_only || store->wal)
        return;
    store->wal_path = derive_wal_path(store->manifest_path);
    store->wal      = bs_adapter_attach_persist_wal_open(store->wal_path.c_str());
}

BsAttachStore* bs_adapter_attach_persist_store_open(const char* manifest_path)
{
    auto* s = static_cast<BsAttachStore*>(attach_malloc(sizeof(BsAttachStore)));
    if (!s)
        return nullptr;
    new (s) BsAttachStore();
    if (!manifest_path || manifest_path[0] == '\0')
    {
        s->memory_only = true;
        return s;
    }
    s->manifest_path = manifest_path;
    if (load_manifest_file(s) != BS_ATTACH_OK)
    {
        bs_adapter_attach_persist_store_close(s);
        return nullptr;
    }
    open_wal(s);
    if (s->wal)
    {
        (void)bs_adapter_attach_persist_wal_recover_unfinished(s->wal, s->batch_epoch);
        (void)bs_adapter_attach_persist_wal_purge_old_segments(s->wal_path.c_str(), s->batch_epoch);
    }
    return s;
}

void bs_adapter_attach_persist_store_close(BsAttachStore* store)
{
    if (!store)
        return;
    if (store->wal)
        bs_adapter_attach_persist_wal_close(store->wal);
    store->wal = nullptr;
    store->~BsAttachStore();
    std::free(store);
}

uint64_t bs_adapter_attach_persist_store_batch_epoch(const BsAttachStore* store)
{
    return store ? store->batch_epoch : 0;
}

int bs_adapter_attach_persist_store_get_revision(const BsAttachStore* store, const char* uri,
                                                 uint64_t* rev_out)
{
    if (!store || !uri || !rev_out)
        return BS_ATTACH_ERR_INVALID_ARG;
    const auto it = store->revisions.find(uri);
    *rev_out      = (it == store->revisions.end()) ? 0 : it->second;
    return BS_ATTACH_OK;
}

static int commit_one(BsAttachStore* store, const char* uri, const char* path, const void* data,
                      size_t len, uint64_t expected_rev)
{
    const auto     it      = store->revisions.find(uri);
    const uint64_t current = (it == store->revisions.end()) ? 0 : it->second;
    if (current != expected_rev)
        return BS_ATTACH_ERR_CONFLICT;

    if (!store->memory_only)
    {
        const int wr = write_file_atomic(path, data, len, should_fsync_canonical(store));
        if (wr != BS_ATTACH_OK)
            return wr;
    }

    store->revisions[uri]       = expected_rev + 1;
    store->canonical_paths[uri] = path;
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_store_get_canonical_path(const BsAttachStore* store, const char* uri,
                                                       char* out_path, size_t out_cap)
{
    if (!store || !uri || !out_path || out_cap == 0)
        return BS_ATTACH_ERR_INVALID_ARG;
    const auto it = store->canonical_paths.find(uri);
    if (it == store->canonical_paths.end())
        return BS_ATTACH_ERR_INVALID_ARG;
    if (it->second.size() + 1 > out_cap)
        return BS_ATTACH_ERR_INVALID_ARG;
    std::memcpy(out_path, it->second.c_str(), it->second.size() + 1);
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_store_commit_per_path(BsAttachStore* store, const char* uri,
                                                    const void* data, size_t len,
                                                    uint64_t expected_rev)
{
    if (!store || !uri || (!data && len > 0))
        return BS_ATTACH_ERR_INVALID_ARG;
    char path[4096];
    if (bs_adapter_attach_persist_uri_to_path(uri, path, sizeof(path)) != BS_ATTACH_OK)
        return BS_ATTACH_ERR_INVALID_ARG;
    const int rc = commit_one(store, uri, path, data, len, expected_rev);
    if (rc != BS_ATTACH_OK)
        return rc;
    ++store->batch_epoch;
    return save_manifest_file(store);
}

void bs_adapter_attach_persist_store_batch_begin(BsAttachStore* store)
{
    if (!store)
        return;
    store->staged.clear();
    store->batch_open = true;
}

int bs_adapter_attach_persist_store_batch_stage(BsAttachStore* store, const char* uri,
                                                const void* data, size_t len, uint64_t expected_rev)
{
    if (!store || !store->batch_open || !uri || (!data && len > 0))
        return BS_ATTACH_ERR_INVALID_ARG;
    char path[4096];
    if (bs_adapter_attach_persist_uri_to_path(uri, path, sizeof(path)) != BS_ATTACH_OK)
        return BS_ATTACH_ERR_INVALID_ARG;

    StagedEntry e;
    e.uri          = uri;
    e.path         = path;
    e.expected_rev = expected_rev;
    e.data.resize(len);
    if (len > 0)
        std::memcpy(e.data.data(), data, len);
    store->staged.push_back(std::move(e));
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_store_batch_commit(BsAttachStore* store)
{
    if (!store || !store->batch_open)
        return BS_ATTACH_ERR_INVALID_ARG;

    for (const auto& e : store->staged)
    {
        const auto     it      = store->revisions.find(e.uri);
        const uint64_t current = (it == store->revisions.end()) ? 0 : it->second;
        if (current != e.expected_rev)
        {
            publish_watch_event(store->batch_epoch + 1, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_CAS,
                                BS_ATTACH_WATCH_RESULT_FAIL);
            bs_adapter_attach_persist_store_batch_abort(store);
            return BS_ATTACH_ERR_CONFLICT;
        }
    }

    const uint64_t next_epoch = store->batch_epoch + 1;
    for (const auto& e : store->staged)
        publish_watch_event(next_epoch, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_CAS,
                            BS_ATTACH_WATCH_RESULT_OK);

    std::vector<BsAttachWalEntry> wal_entries;
    std::vector<std::string>      staging_paths;
    wal_entries.resize(store->staged.size());
    staging_paths.resize(store->staged.size());

    for (size_t i = 0; i < store->staged.size(); ++i)
    {
        const auto&    e                = store->staged[i];
        const uint64_t new_rev          = e.expected_rev + 1;
        staging_paths[i]                = make_staging_path(e.path, next_epoch, new_rev);
        wal_entries[i].uri              = e.uri.c_str();
        wal_entries[i].staging_path     = staging_paths[i].c_str();
        wal_entries[i].expected_rev     = e.expected_rev;
        wal_entries[i].new_rev          = new_rev;
        wal_entries[i].payload_checksum = crc32_payload(e.data.data(), e.data.size());
    }

    if (!store->memory_only)
    {
        open_wal(store);
        if (store->wal)
        {
            const int wr = bs_adapter_attach_persist_wal_append_batch(
                store->wal, next_epoch, wal_entries.data(), wal_entries.size());
            if (wr != BS_ATTACH_OK)
            {
                for (const auto& e : store->staged)
                    publish_watch_event(next_epoch, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_WAL_FSYNC,
                                        BS_ATTACH_WATCH_RESULT_FAIL);
                bs_adapter_attach_persist_store_batch_abort(store);
                return wr;
            }
            for (const auto& e : store->staged)
                publish_watch_event(next_epoch, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_WAL_FSYNC,
                                    BS_ATTACH_WATCH_RESULT_OK);
        }

        for (size_t i = 0; i < store->staged.size(); ++i)
        {
            const auto& e = store->staged[i];
            const int wr = write_file_atomic(staging_paths[i].c_str(), e.data.data(), e.data.size(),
                                             should_fsync_canonical(store));
            if (wr != BS_ATTACH_OK)
            {
                publish_watch_event(next_epoch, e.uri.c_str(),
                                    BS_ATTACH_WATCH_STAGE_CANONICAL_WRITE,
                                    BS_ATTACH_WATCH_RESULT_FAIL);
                bs_adapter_attach_persist_store_batch_abort(store);
                return wr;
            }
            publish_watch_event(next_epoch, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_CANONICAL_WRITE,
                                BS_ATTACH_WATCH_RESULT_OK);
        }
    }

    for (size_t i = 0; i < store->staged.size(); ++i)
    {
        const auto& e                 = store->staged[i];
        store->revisions[e.uri]       = e.expected_rev + 1;
        store->canonical_paths[e.uri] = staging_paths[i];
    }

    store->batch_epoch = next_epoch;
    store->batch_open  = false;
    store->staged.clear();

    const int man = save_manifest_file(store);
    if (man != BS_ATTACH_OK)
    {
        for (const auto& e : store->staged)
            publish_watch_event(next_epoch, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_MANIFEST_FLIP,
                                BS_ATTACH_WATCH_RESULT_FAIL);
        return man;
    }
    for (const auto& e : store->staged)
        publish_watch_event(next_epoch, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_MANIFEST_FLIP,
                            BS_ATTACH_WATCH_RESULT_OK);

    if (!store->memory_only && store->wal)
    {
        const int wrc = bs_adapter_attach_persist_wal_mark_committed(store->wal, next_epoch);
        for (const auto& e : store->staged)
            publish_watch_event(next_epoch, e.uri.c_str(), BS_ATTACH_WATCH_STAGE_WAL_COMMIT,
                                (wrc == BS_ATTACH_OK) ? BS_ATTACH_WATCH_RESULT_OK
                                                      : BS_ATTACH_WATCH_RESULT_FAIL);
    }

    return BS_ATTACH_OK;
}

void bs_adapter_attach_persist_store_batch_abort(BsAttachStore* store)
{
    if (!store)
        return;
    store->staged.clear();
    store->batch_open = false;
}
