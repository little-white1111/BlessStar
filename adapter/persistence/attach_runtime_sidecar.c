#include "bs/adapter/persistence/attach_runtime_sidecar.h"

#include "bs/kernel/common/bs_wait_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attach_crc32.h"

typedef struct SidecarCollectCtx
{
    BsAttachRuntimeSidecarEntry* entries;
    size_t                       count;
    size_t                       cap;
    BsAttachStore*               store;
    int                          rc;
} SidecarCollectCtx;

static int sidecar_path_for_manifest(const char* manifest_path, char* out, size_t out_cap)
{
    if (!manifest_path || !out || out_cap == 0)
        return BS_ATTACH_ERR_INVALID_ARG;
    const size_t n          = strlen(manifest_path);
    const size_t suffix_len = sizeof(".runtime.ckpt") - 1; /* NOLINT(bugprone-sizeof-expression) */
    if (n + suffix_len + 1 > out_cap)
        return BS_ATTACH_ERR_LIMIT;
    memcpy(out, manifest_path, n);
    memcpy(out + n, ".runtime.ckpt", suffix_len + 1);
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_sidecar_path_for_manifest(const char* manifest_path, char* out,
                                                        size_t out_cap)
{
    return sidecar_path_for_manifest(manifest_path, out, out_cap);
}

int bs_adapter_attach_persist_read_file_digest(const char* path, uint32_t* digest_out)
{
    if (!path || !digest_out)
        return BS_ATTACH_ERR_INVALID_ARG;

    FILE* f = fopen(path, "rb");
    if (!f)
        return BS_ATTACH_ERR_IO;
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return BS_ATTACH_ERR_IO;
    }
    fseek(f, 0, SEEK_SET);
    if (sz == 0)
    {
        *digest_out = bs_adapter_attach_persist_crc32("", 0);
        fclose(f);
        return BS_ATTACH_OK;
    }

    uint8_t* data = (uint8_t*)malloc((size_t)sz);
    if (!data)
    {
        fclose(f);
        return BS_ATTACH_ERR_OOM;
    }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz)
    {
        free(data);
        fclose(f);
        return BS_ATTACH_ERR_IO;
    }
    fclose(f);
    *digest_out = bs_adapter_attach_persist_crc32(data, (size_t)sz);
    free(data);
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_manifest_file_digest(const char* manifest_path, uint32_t* digest_out)
{
    return bs_adapter_attach_persist_read_file_digest(manifest_path, digest_out);
}

static int collect_entry(const char* uri, uint64_t rev, void* user_ctx)
{
    SidecarCollectCtx* ctx = (SidecarCollectCtx*)user_ctx;
    if (!ctx || !uri || !ctx->store)
        return BS_ATTACH_ERR_INVALID_ARG;

    char canonical[4096];
    if (bs_adapter_attach_persist_store_get_canonical_path(ctx->store, uri, canonical,
                                                           sizeof(canonical)) != BS_ATTACH_OK)
    {
        ctx->rc = BS_ATTACH_ERR_IO;
        return BS_ATTACH_ERR_IO;
    }

    uint32_t payload_digest = 0;
    if (bs_adapter_attach_persist_read_file_digest(canonical, &payload_digest) != BS_ATTACH_OK)
    {
        ctx->rc = BS_ATTACH_ERR_IO;
        return BS_ATTACH_ERR_IO;
    }

    if (ctx->count >= ctx->cap)
    {
        ctx->rc = BS_ATTACH_ERR_LIMIT;
        return BS_ATTACH_ERR_LIMIT;
    }

    ctx->entries[ctx->count].uri            = uri;
    ctx->entries[ctx->count].revision       = rev;
    ctx->entries[ctx->count].payload_digest = payload_digest;
    ++ctx->count;
    return BS_ATTACH_OK;
}

static int append_bytes(uint8_t* body, size_t body_cap, size_t* off, const void* data, size_t len)
{
    if (*off + len > body_cap)
        return BS_ATTACH_ERR_LIMIT;
    memcpy(body + *off, data, len);
    *off += len;
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_sidecar_invalidate(const char* manifest_path)
{
    if (!manifest_path)
        return BS_ATTACH_ERR_INVALID_ARG;
    bs_wait_trace_path("persist_io:sidecar_invalidate", manifest_path);
    const int io_t0 = bs_wait_trace_hang_begin("persist_io:sidecar_invalidate");
    char      path[4096];
    if (sidecar_path_for_manifest(manifest_path, path, sizeof(path)) != BS_ATTACH_OK)
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:sidecar_invalidate", io_t0);
        return BS_ATTACH_ERR_IO;
    }
    (void)remove(path);
    if (io_t0 >= 0)
        bs_wait_trace_hang_end("persist_io:sidecar_invalidate", io_t0);
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_sidecar_write(const char* manifest_path, const BsAttachStore* store,
                                            uint32_t flags)
{
    if (!manifest_path || !store)
        return BS_ATTACH_ERR_INVALID_ARG;

    BsAttachRuntimeSidecarEntry stack_entries[64];
    SidecarCollectCtx collect = {stack_entries, 0, 64, (BsAttachStore*)store, BS_ATTACH_OK};

    if (bs_adapter_attach_persist_store_foreach_uri(store, collect_entry, &collect) !=
            BS_ATTACH_OK ||
        collect.rc != BS_ATTACH_OK)
        return collect.rc;

    uint32_t manifest_digest = 0;
    if (bs_adapter_attach_persist_manifest_file_digest(manifest_path, &manifest_digest) !=
        BS_ATTACH_OK)
        return BS_ATTACH_ERR_IO;

    size_t body_cap = 32;
    for (size_t i = 0; i < collect.count; ++i)
        body_cap += 2 + strlen(collect.entries[i].uri) + 8 + 4;

    uint8_t* body = (uint8_t*)malloc(body_cap);
    if (!body)
        return BS_ATTACH_ERR_OOM;

    size_t         off     = 0;
    const uint32_t magic   = BS_ATTACH_SIDECAR_MAGIC;
    const uint16_t version = BS_ATTACH_SIDECAR_VERSION;
    const uint64_t epoch   = bs_adapter_attach_persist_store_batch_epoch(store);
    const uint32_t count   = (uint32_t)collect.count;

    if (append_bytes(body, body_cap, &off, &magic, 4) != BS_ATTACH_OK ||
        append_bytes(body, body_cap, &off, &version, 2) != BS_ATTACH_OK ||
        append_bytes(body, body_cap, &off, &flags, 2) != BS_ATTACH_OK ||
        append_bytes(body, body_cap, &off, &epoch, 8) != BS_ATTACH_OK ||
        append_bytes(body, body_cap, &off, &manifest_digest, 4) != BS_ATTACH_OK ||
        append_bytes(body, body_cap, &off, &count, 4) != BS_ATTACH_OK)
    {
        free(body);
        return BS_ATTACH_ERR_LIMIT;
    }

    for (size_t i = 0; i < collect.count; ++i)
    {
        const char*  uri  = collect.entries[i].uri;
        const size_t ulen = strlen(uri);
        if (ulen > 0xFFFFu)
        {
            free(body);
            return BS_ATTACH_ERR_LIMIT;
        }
        const uint16_t uri_len = (uint16_t)ulen;
        const uint64_t rev     = collect.entries[i].revision;
        const uint32_t pdig    = collect.entries[i].payload_digest;
        if (append_bytes(body, body_cap, &off, &uri_len, 2) != BS_ATTACH_OK ||
            append_bytes(body, body_cap, &off, uri, ulen) != BS_ATTACH_OK ||
            append_bytes(body, body_cap, &off, &rev, 8) != BS_ATTACH_OK ||
            append_bytes(body, body_cap, &off, &pdig, 4) != BS_ATTACH_OK)
        {
            free(body);
            return BS_ATTACH_ERR_LIMIT;
        }
    }

    const uint32_t file_crc = bs_adapter_attach_persist_crc32(body, off);
    if (append_bytes(body, body_cap, &off, &file_crc, 4) != BS_ATTACH_OK)
    {
        free(body);
        return BS_ATTACH_ERR_LIMIT;
    }

    char path[4096];
    if (sidecar_path_for_manifest(manifest_path, path, sizeof(path)) != BS_ATTACH_OK)
    {
        free(body);
        return BS_ATTACH_ERR_IO;
    }

    char tmp[4096];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
    {
        free(body);
        return BS_ATTACH_ERR_LIMIT;
    }

    FILE* f = fopen(tmp, "wb");
    if (!f)
    {
        free(body);
        return BS_ATTACH_ERR_IO;
    }
    const int wr = (fwrite(body, 1, off, f) == off) ? BS_ATTACH_OK : BS_ATTACH_ERR_IO;
    fclose(f);
    free(body);
    if (wr != BS_ATTACH_OK)
    {
        remove(tmp);
        return wr;
    }
    (void)remove(path);
    if (rename(tmp, path) != 0)
    {
        remove(tmp);
        return BS_ATTACH_ERR_IO;
    }
    return BS_ATTACH_OK;
}

static int read_u32(const uint8_t* data, size_t body_len, size_t* off, uint32_t* out)
{
    if (*off + 4 > body_len)
        return 0;
    memcpy(out, data + *off, 4);
    *off += 4;
    return 1;
}

static int read_u16(const uint8_t* data, size_t body_len, size_t* off, uint16_t* out)
{
    if (*off + 2 > body_len)
        return 0;
    memcpy(out, data + *off, 2);
    *off += 2;
    return 1;
}

static int read_u64(const uint8_t* data, size_t body_len, size_t* off, uint64_t* out)
{
    if (*off + 8 > body_len)
        return 0;
    memcpy(out, data + *off, 8);
    *off += 8;
    return 1;
}

int bs_adapter_attach_persist_sidecar_validate(const char*          manifest_path,
                                               const BsAttachStore* store, uint32_t required_flags)
{
    if (!manifest_path || !store)
        return 0;

    char path[4096];
    if (sidecar_path_for_manifest(manifest_path, path, sizeof(path)) != BS_ATTACH_OK)
        return 0;

    FILE* f = fopen(path, "rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    const long file_size = ftell(f);
    if (file_size < 28)
    {
        fclose(f);
        return 0;
    }
    fseek(f, 0, SEEK_SET);
    uint8_t* data = (uint8_t*)malloc((size_t)file_size);
    if (!data)
    {
        fclose(f);
        return 0;
    }
    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size)
    {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    const size_t   body_len   = (size_t)file_size - 4;
    const uint32_t expect_crc = bs_adapter_attach_persist_crc32(data, body_len);
    uint32_t       got_crc    = 0;
    memcpy(&got_crc, data + body_len, 4);
    if (expect_crc != got_crc)
    {
        free(data);
        return 0;
    }

    size_t   off             = 0;
    uint32_t magic           = 0;
    uint16_t version         = 0;
    uint16_t flags           = 0;
    uint64_t epoch           = 0;
    uint32_t manifest_digest = 0;
    uint32_t count           = 0;

    if (!read_u32(data, body_len, &off, &magic) || magic != BS_ATTACH_SIDECAR_MAGIC ||
        !read_u16(data, body_len, &off, &version) || version != BS_ATTACH_SIDECAR_VERSION ||
        !read_u16(data, body_len, &off, &flags) || (flags & required_flags) != required_flags ||
        !read_u64(data, body_len, &off, &epoch) ||
        !read_u32(data, body_len, &off, &manifest_digest) ||
        !read_u32(data, body_len, &off, &count))
    {
        free(data);
        return 0;
    }

    if (epoch != bs_adapter_attach_persist_store_batch_epoch(store))
    {
        free(data);
        return 0;
    }

    uint32_t live_digest = 0;
    if (bs_adapter_attach_persist_manifest_file_digest(manifest_path, &live_digest) !=
            BS_ATTACH_OK ||
        live_digest != manifest_digest)
    {
        free(data);
        return 0;
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        uint16_t uri_len = 0;
        if (!read_u16(data, body_len, &off, &uri_len) || off + uri_len + 8 + 4 > body_len)
        {
            free(data);
            return 0;
        }
        char uri[4096];
        if (uri_len + 1 > sizeof(uri))
        {
            free(data);
            return 0;
        }
        memcpy(uri, data + off, uri_len);
        uri[uri_len] = '\0';
        off += uri_len;
        uint64_t rev  = 0;
        uint32_t pdig = 0;
        if (!read_u64(data, body_len, &off, &rev) || !read_u32(data, body_len, &off, &pdig))
        {
            free(data);
            return 0;
        }

        uint64_t live_rev = 0;
        if (bs_adapter_attach_persist_store_get_revision(store, uri, &live_rev) != BS_ATTACH_OK ||
            live_rev != rev)
        {
            free(data);
            return 0;
        }

        char canonical[4096];
        if (bs_adapter_attach_persist_store_get_canonical_path(store, uri, canonical,
                                                               sizeof(canonical)) != BS_ATTACH_OK)
        {
            free(data);
            return 0;
        }
        uint32_t live_pdig = 0;
        if (bs_adapter_attach_persist_read_file_digest(canonical, &live_pdig) != BS_ATTACH_OK ||
            live_pdig != pdig)
        {
            free(data);
            return 0;
        }
    }

    const int ok = (off == body_len) ? 1 : 0;
    free(data);
    return ok;
}
