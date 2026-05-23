#include "bs/adapter/persistence/attach_store.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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
    bool                                         memory_only = false;
    uint64_t                                     batch_epoch = 0;
    std::unordered_map<std::string, uint64_t>    revisions;
    std::unordered_map<std::string, std::string> canonical_paths;
    std::vector<StagedEntry>                     staged;
    bool                                         batch_open = false;
};

static BsAttachMallocFn g_malloc_hook = nullptr;

static void* attach_malloc(size_t n)
{
    if (g_malloc_hook)
        return g_malloc_hook(n);
    return std::malloc(n);
}

extern "C" void bs_attach_store_set_malloc_hook(BsAttachMallocFn fn)
{
    g_malloc_hook = fn;
}

extern "C" void bs_attach_store_reset_malloc_hook(void)
{
    g_malloc_hook = nullptr;
}

static int write_file_atomic(const char* path, const void* data, size_t len)
{
    if (!path)
        return BS_ATTACH_ERR_INVALID_ARG;
    std::string tmp = std::string(path) + ".bs.tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return BS_ATTACH_ERR_IO;
        if (len > 0)
        {
            out.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
            if (!out)
                return BS_ATTACH_ERR_IO;
        }
    }
    (void)std::remove(path);
    if (std::rename(tmp.c_str(), path) != 0)
    {
        std::remove(tmp.c_str());
        return BS_ATTACH_ERR_IO;
    }
    return BS_ATTACH_OK;
}

static int load_manifest_file(BsAttachStore* store)
{
    if (!store || store->memory_only)
        return BS_ATTACH_OK;
    std::ifstream in(store->manifest_path);
    if (!in)
    {
        store->revisions.clear();
        store->canonical_paths.clear();
        store->batch_epoch = 0;
        return BS_ATTACH_OK;
    }
    store->revisions.clear();
    store->canonical_paths.clear();
    std::string line;
    std::string current_uri;
    while (std::getline(in, line))
    {
        if (line.size() > BS_ATTACH_MAX_MANIFEST_LINE)
            return BS_ATTACH_ERR_LIMIT;
        if (line.empty() || line[0] == '#')
            continue;
        if (line.rfind("batch_epoch=", 0) == 0)
        {
            store->batch_epoch =
                static_cast<uint64_t>(std::strtoull(line.c_str() + 12, nullptr, 10));
            continue;
        }
        if (line.rfind("[uri=", 0) == 0 && line.size() > 6 && line.back() == ']')
        {
            current_uri = line.substr(5, line.size() - 6);
            continue;
        }
        if (!current_uri.empty() && line.rfind("revision=", 0) == 0)
        {
            store->revisions[current_uri] =
                static_cast<uint64_t>(std::strtoull(line.c_str() + 9, nullptr, 10));
            continue;
        }
        if (!current_uri.empty() && line.rfind("path=", 0) == 0)
        {
            store->canonical_paths[current_uri] = line.substr(5);
        }
    }
    return BS_ATTACH_OK;
}

static int save_manifest_file(const BsAttachStore* store)
{
    if (!store || store->memory_only)
        return BS_ATTACH_OK;
    std::ostringstream body;
    body << "# BlessStar manifest v1\n";
    body << "batch_epoch=" << store->batch_epoch << "\n";
    for (const auto& kv : store->revisions)
    {
        body << "[uri=" << kv.first << "]\n";
        body << "revision=" << kv.second << "\n";
        const auto pit = store->canonical_paths.find(kv.first);
        if (pit != store->canonical_paths.end())
            body << "path=" << pit->second << "\n";
    }
    const std::string tmp = store->manifest_path + ".bs.tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return BS_ATTACH_ERR_IO;
        const std::string s = body.str();
        out.write(s.data(), static_cast<std::streamsize>(s.size()));
        if (!out)
            return BS_ATTACH_ERR_IO;
    }
    (void)std::remove(store->manifest_path.c_str());
    if (std::rename(tmp.c_str(), store->manifest_path.c_str()) != 0)
        return BS_ATTACH_ERR_IO;
    return BS_ATTACH_OK;
}

BsAttachStore* bs_attach_store_open(const char* manifest_path)
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
        bs_attach_store_close(s);
        return nullptr;
    }
    return s;
}

void bs_attach_store_close(BsAttachStore* store)
{
    if (!store)
        return;
    store->~BsAttachStore();
    std::free(store);
}

uint64_t bs_attach_store_batch_epoch(const BsAttachStore* store)
{
    return store ? store->batch_epoch : 0;
}

int bs_attach_store_get_revision(const BsAttachStore* store, const char* uri, uint64_t* rev_out)
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
        const int wr = write_file_atomic(path, data, len);
        if (wr != BS_ATTACH_OK)
            return wr;
    }

    store->revisions[uri]       = expected_rev + 1;
    store->canonical_paths[uri] = path;
    return BS_ATTACH_OK;
}

int bs_attach_store_get_canonical_path(const BsAttachStore* store, const char* uri, char* out_path,
                                       size_t out_cap)
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

int bs_attach_store_commit_per_path(BsAttachStore* store, const char* uri, const void* data,
                                    size_t len, uint64_t expected_rev)
{
    if (!store || !uri || (!data && len > 0))
        return BS_ATTACH_ERR_INVALID_ARG;
    char path[4096];
    if (bs_attach_uri_to_path(uri, path, sizeof(path)) != BS_ATTACH_OK)
        return BS_ATTACH_ERR_INVALID_ARG;
    const int rc = commit_one(store, uri, path, data, len, expected_rev);
    if (rc != BS_ATTACH_OK)
        return rc;
    ++store->batch_epoch;
    return save_manifest_file(store);
}

void bs_attach_store_batch_begin(BsAttachStore* store)
{
    if (!store)
        return;
    store->staged.clear();
    store->batch_open = true;
}

int bs_attach_store_batch_stage(BsAttachStore* store, const char* uri, const void* data, size_t len,
                                uint64_t expected_rev)
{
    if (!store || !store->batch_open || !uri || (!data && len > 0))
        return BS_ATTACH_ERR_INVALID_ARG;
    char path[4096];
    if (bs_attach_uri_to_path(uri, path, sizeof(path)) != BS_ATTACH_OK)
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

int bs_attach_store_batch_commit(BsAttachStore* store)
{
    if (!store || !store->batch_open)
        return BS_ATTACH_ERR_INVALID_ARG;

    for (const auto& e : store->staged)
    {
        const auto     it      = store->revisions.find(e.uri);
        const uint64_t current = (it == store->revisions.end()) ? 0 : it->second;
        if (current != e.expected_rev)
        {
            bs_attach_store_batch_abort(store);
            return BS_ATTACH_ERR_CONFLICT;
        }
    }

    for (const auto& e : store->staged)
    {
        if (!store->memory_only)
        {
            const int wr = write_file_atomic(e.path.c_str(), e.data.data(), e.data.size());
            if (wr != BS_ATTACH_OK)
            {
                bs_attach_store_batch_abort(store);
                return wr;
            }
        }
        store->revisions[e.uri]       = e.expected_rev + 1;
        store->canonical_paths[e.uri] = e.path;
    }

    ++store->batch_epoch;
    store->batch_open = false;
    store->staged.clear();
    return save_manifest_file(store);
}

void bs_attach_store_batch_abort(BsAttachStore* store)
{
    if (!store)
        return;
    store->staged.clear();
    store->batch_open = false;
}
