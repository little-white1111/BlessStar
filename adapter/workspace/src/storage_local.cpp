/*
 * LocalDirectStorage — StorageBackend 实现 (裸文件 I/O).
 *
 * ADR-workspace加固:
 *   不变量 #3 (路径安全): 拒绝 ".." 路径穿越。
 *   不变量 #4 (测试无侵入): BS_TESTING 下自动启用。
 */

#include "bs/adapter/workspace/storage_local.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <fileapi.h>
#include <handleapi.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ─── Internal context ─────────────────────────────────────────────── */

struct LocalDirectCtx {
    std::string project_root;
};

/* ─── Path safety ──────────────────────────────────────────────────── */

int storage_path_safe(const char* path)
{
    if (!path) return -EINVAL;
    /* Reject path traversal: ".." as a path component */
    const char* p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.' &&
            (p == path || p[-1] == '/' || p[-1] == '\\') &&
            (p[2] == '\0' || p[2] == '/' || p[2] == '\\')) {
            return -EINVAL;
        }
        ++p;
    }
    return 0;
}

/* ─── Helpers to extract impl_ctx ──────────────────────────────────── */

static LocalDirectCtx* get_ctx(void* ctx)
{
    if (!ctx) return NULL;
    auto* sb = static_cast<StorageBackend*>(ctx);
    return static_cast<LocalDirectCtx*>(sb->impl_ctx);
}

/* ─── Read ─────────────────────────────────────────────────────────── */

static int local_read(void* ctx, const char* path,
                       uint8_t** out_data, size_t* out_len)
{
    if (!ctx || !path || !out_data || !out_len) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* lctx = get_ctx(ctx);
    if (!lctx) return -EINVAL;
    std::string full = lctx->project_root + "/" + path;

#ifdef _WIN32
    HANDLE h = CreateFileA(full.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -ENOENT;

    LARGE_INTEGER li;
    if (!GetFileSizeEx(h, &li)) {
        CloseHandle(h);
        return -EIO;
    }
    size_t size = (size_t)li.QuadPart;

    uint8_t* buf = (uint8_t*)malloc(size + 1);
    if (!buf) { CloseHandle(h); return -ENOMEM; }

    DWORD bytes_read;
    if (!ReadFile(h, buf, (DWORD)size, &bytes_read, NULL)) {
        free(buf); CloseHandle(h);
        return -EIO;
    }
    buf[size] = '\0';
    *out_data = buf;
    *out_len  = size + 1;
    CloseHandle(h);
    return 0;
#else
    FILE* f = fopen(full.c_str(), "rb");
    if (!f) return -ENOENT;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return -EIO; }
    rewind(f);

    uint8_t* buf = (uint8_t*)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return -ENOMEM; }

    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    *out_data = buf;
    *out_len  = n + 1;
    return 0;
#endif
}

/* ─── Write ────────────────────────────────────────────────────────── */

static int local_write(void* ctx, const char* path,
                        const uint8_t* data, size_t len)
{
    if (!ctx || !path || !data) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* lctx = get_ctx(ctx);
    if (!lctx) return -EINVAL;
    std::string full = lctx->project_root + "/" + path;

#ifdef _WIN32
    HANDLE h = CreateFileA(full.c_str(), GENERIC_WRITE, 0,
                           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -EIO;

    DWORD written;
    if (!WriteFile(h, data, (DWORD)(len > 0 ? len - 1 : 0), &written, NULL)) {
        CloseHandle(h);
        return -EIO;
    }
    /* fsync */
    if (!FlushFileBuffers(h)) {
        CloseHandle(h);
        return -EIO;
    }
    CloseHandle(h);
    return 0;
#else
    FILE* f = fopen(full.c_str(), "wb");
    if (!f) return -EIO;

    size_t write_len = (len > 0) ? len - 1 : 0;
    if (fwrite(data, 1, write_len, f) != write_len) {
        fclose(f);
        return -EIO;
    }
    if (fflush(f) != 0) { fclose(f); return -EIO; }
#ifdef __linux__
    if (fdatasync(fileno(f)) != 0) { fclose(f); return -EIO; }
#endif
    fclose(f);
    return 0;
#endif
}

/* ─── Exists ───────────────────────────────────────────────────────── */

static int local_exists(void* ctx, const char* path)
{
    if (!ctx || !path) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* lctx = get_ctx(ctx);
    if (!lctx) return -EINVAL;
    std::string full = lctx->project_root + "/" + path;

#ifdef _WIN32
    DWORD attr = GetFileAttributesA(full.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
#else
    struct stat st;
    return (stat(full.c_str(), &st) == 0) ? 1 : 0;
#endif
}

/* ─── Remove ───────────────────────────────────────────────────────── */

static int local_remove(void* ctx, const char* path)
{
    if (!ctx || !path) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* lctx = get_ctx(ctx);
    if (!lctx) return -EINVAL;
    std::string full = lctx->project_root + "/" + path;

    if (remove(full.c_str()) != 0) {
        if (errno == ENOENT) return -ENOENT;
        return -EIO;
    }
    return 0;
}

/* ─── Destroy ──────────────────────────────────────────────────────── */

static void local_destroy(void* ctx)
{
    if (!ctx) return;
    auto* sb = static_cast<StorageBackend*>(ctx);
    if (sb->impl_ctx) {
        delete static_cast<LocalDirectCtx*>(sb->impl_ctx);
        sb->impl_ctx = NULL;
    }
}

/* ─── Constructor ──────────────────────────────────────────────────── */

StorageBackend* LocalDirectStorage_new(const char* project_root)
{
    if (!project_root) return NULL;

    auto* ctx = new (std::nothrow) LocalDirectCtx;
    if (!ctx) return NULL;
    ctx->project_root = project_root;

    auto* sb = new (std::nothrow) StorageBackend;
    if (!sb) { delete ctx; return NULL; }

    sb->impl_ctx  = ctx;
    sb->read      = local_read;
    sb->write     = local_write;
    sb->exists    = local_exists;
    sb->remove_fn = local_remove;
    sb->destroy   = local_destroy;

    return sb;
}
