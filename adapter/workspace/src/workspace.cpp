/*
 * Workspace Service — lifecycle and source management.
 *
 * ADR-全链路接通 不变量 #2 (Workspace 数据主权):
 *   所有配置文件读写必须经过 bs_workspace_* C ABI。
 * ADR-全链路接通 不变量 #6 (元数据一致性):
 *   .blessstar/ 目录必须时刻与 src/ 内容一致。
 */

#include "bs/adapter/workspace/workspace.h"
#include "bs/adapter/workspace/storage_backend.h"
#include "bs/adapter/workspace/storage_local.h"
#ifndef BS_TESTING
#include "bs/adapter/workspace/storage_persist.h"
#include <bs/adapter/persistence/attach_store.h>
#endif

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

/* StorageBackend selection via BSTORAGE env var or BS_TESTING */
static StorageBackend* create_storage(const std::string& project_root)
{
    const char* env = getenv("BSTORAGE");

    /* BSTORAGE=local → force LocalDirectStorage */
    if (env && strcmp(env, "local") == 0) {
        return LocalDirectStorage_new(project_root.c_str());
    }

#ifndef BS_TESTING
    /* BSTORAGE=persist → force PersistStoreStorage */
    if (env && strcmp(env, "persist") == 0) {
        std::string manifest = project_root + "/.blessstar/store_manifest";
        BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest.c_str());
        if (store) {
            return PersistStoreStorage_new(project_root.c_str(), store);
        }
        /* If store open fails, fall through to production default */
    }

    /* ADR-workspace加固 不变量 #5: 生产默认 — PersistStoreStorage */
    std::string manifest = project_root + "/.blessstar/store_manifest";
    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest.c_str());
    if (store) {
        return PersistStoreStorage_new(project_root.c_str(), store);
    }
    /* Fallback to LocalDirect if store can't be opened (e.g. first run) */
    return LocalDirectStorage_new(project_root.c_str());
#else
    /* ADR-workspace加固 不变量 #4: 测试无侵入 — BS_TESTING 自动启用 LocalDirect */
    (void)env; /* BSTORAGE=persist ignored under BS_TESTING (no io_worker) */
    return LocalDirectStorage_new(project_root.c_str());
#endif
}

#ifdef _WIN32
#include <direct.h>
#include <fileapi.h>
#include <handleapi.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define access(path, mode) _access(path, 0)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ─── Internal structures ──────────────────────────────────────────── */
/* SourceEntry and bs_workspace_t are now defined in workspace.h
   (inside #ifdef __cplusplus) to make them visible to workspace_build.cpp
   and workspace_export.cpp. */

/* ─── File system helpers ──────────────────────────────────────────── */

static bool path_exists(const std::string& path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

static int create_dir(const std::string& path)
{
#ifdef _WIN32
    if (!CreateDirectoryA(path.c_str(), NULL)) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
        return -EIO;
    }
#else
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        return -EIO;
    }
#endif
    return 0;
}

static std::string join_path(const std::string& base, const std::string& sub)
{
    if (base.back() == '/' || base.back() == '\\')
        return base + sub;
    return base + "/" + sub;
}

/* ─── Lifecycle ────────────────────────────────────────────────────── */

bs_workspace_t* bs_workspace_create(const char* project_root)
{
    if (!project_root) return NULL;

    bs_workspace_t* ws = new (std::nothrow) bs_workspace_t;
    if (!ws) return NULL;

    ws->project_root = project_root;
    ws->storage      = NULL;

    /* Create .blessstar directory structure */
    std::string meta_dir = join_path(project_root, ".blessstar");
    if (create_dir(meta_dir) != 0) {
        delete ws;
        return NULL;
    }
    create_dir(join_path(meta_dir, "schema"));
    create_dir(join_path(meta_dir, "history"));
    create_dir(join_path(project_root, "src"));
    create_dir(join_path(project_root, "dist"));

    /* ADR-workspace加固: 创建 StorageBackend */
    ws->storage = create_storage(project_root);

    return ws;
}

int bs_workspace_open(bs_workspace_t* ws)
{
    if (!ws) return -EINVAL;
    /* Check that .blessstar/workspace.yaml exists via storage */
    std::string ws_yaml = ".blessstar/workspace.yaml";
    if (!ws->storage || !ws->storage->exists ||
        ws->storage->exists(ws->storage, ws_yaml.c_str()) != 1) {
        return -ENOENT;
    }
    /* For MVP, we don't parse YAML — just mark as open */
    return 0;
}

/* ─── Source management ────────────────────────────────────────────── */

int bs_workspace_add_source(bs_workspace_t* ws, const char* rel_path,
                             const char* format)
{
    if (!ws || !rel_path) return -EINVAL;

    SourceEntry entry;
    entry.rel_path = rel_path;
    entry.format   = format ? format : "auto";

    ws->sources.push_back(entry);
    return 0;
}

int bs_workspace_remove_source(bs_workspace_t* ws, const char* rel_path)
{
    if (!ws || !rel_path) return -EINVAL;

    for (auto it = ws->sources.begin(); it != ws->sources.end(); ++it) {
        if (it->rel_path == rel_path) {
            ws->sources.erase(it);
            return 0;
        }
    }
    return -ENOENT;
}

/* ─── Metadata ─────────────────────────────────────────────────────── */

const char* bs_workspace_get_project_root(bs_workspace_t* ws)
{
    return ws ? ws->project_root.c_str() : NULL;
}

int bs_workspace_get_source_count(bs_workspace_t* ws)
{
    return ws ? (int)ws->sources.size() : -EINVAL;
}

void bs_workspace_destroy(bs_workspace_t* ws)
{
    if (ws) {
        if (ws->storage) {
            if (ws->storage->destroy) {
                ws->storage->destroy(ws->storage);
            }
            delete ws->storage;
        }
        ws->storage = NULL;
    }
    delete ws;
}

/* ─── Stubs for build/export/history — defined in separate files ───── */

/* ─── Read / Write ─────────────────────────────────────────────────── */

int bs_workspace_read(bs_workspace_t* ws, const char* src_name,
                      uint8_t** out_data, size_t* out_len)
{
    if (!ws || !src_name || !out_data || !out_len) return -EINVAL;

    /* Find the source */
    int found = 0;
    for (size_t i = 0; i < ws->sources.size(); ++i) {
        if (ws->sources[i].rel_path == src_name) { found = 1; break; }
    }
    if (!found) return -ENOENT;

    /* Read via StorageBackend */
    if (!ws->storage || !ws->storage->read) return -EIO;
    return ws->storage->read(ws->storage, src_name, out_data, out_len);
}

int bs_workspace_write(bs_workspace_t* ws, const char* src_name,
                       const uint8_t* data, size_t len)
{
    if (!ws || !src_name || !data) return -EINVAL;

    /* Find the source */
    int found = 0;
    for (size_t i = 0; i < ws->sources.size(); ++i) {
        if (ws->sources[i].rel_path == src_name) { found = 1; break; }
    }
    if (!found) return -ENOENT;

    /* Write via StorageBackend */
    if (!ws->storage || !ws->storage->write) return -EIO;
    return ws->storage->write(ws->storage, src_name, data, len);
}
