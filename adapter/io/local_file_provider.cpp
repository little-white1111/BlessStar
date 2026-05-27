#include "bs/adapter/io/local_file_provider.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <string>
#include <vector>

struct LocalFileProvider
{
    IoProviderBinding binding{};
};

static char* dup_cstr(const char* s)
{
    if (!s)
        return nullptr;
    const size_t n = std::strlen(s);
    auto*        p = static_cast<char*>(std::malloc(n + 1));
    if (!p)
        return nullptr;
    std::memcpy(p, s, n + 1);
    return p;
}

static bool uri_rest_is_windows_drive(const char* rest)
{
    return rest[2] == ':' &&
           ((rest[1] >= 'A' && rest[1] <= 'Z') || (rest[1] >= 'a' && rest[1] <= 'z'));
}

static int file_uri_to_path(const char* uri, std::string* out_path)
{
    if (!uri || !out_path)
        return BS_IO_ERR_INVALID_ARG;
    if (std::strncmp(uri, "file://", 7) != 0)
        return BS_IO_ERR_INVALID_URI;

    const char* rest = uri + 7;
    if (rest[0] == '/')
    {
        /* file:///absolute, file:///C:/..., file:////absolute (file:/// + /path), file:////unc */
        if (rest[1] == '/')
        {
            if (rest[2] == '/')
                *out_path = rest + 2;
            else if (uri_rest_is_windows_drive(rest))
                *out_path = rest + 1;
            else
                *out_path = rest + 1;
        }
        else if (uri_rest_is_windows_drive(rest))
            *out_path = rest + 1;
        else
            *out_path = rest;
    }
    else
    {
        *out_path = rest;
    }
    if (out_path->empty())
        return BS_IO_ERR_INVALID_URI;
    return BS_IO_OK;
}

static void detect_bom_encoding(const uint8_t* bytes, size_t nbytes, char** encoding_hint)
{
    if (nbytes >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
    {
        *encoding_hint = dup_cstr("utf-8-bom");
        return;
    }
    if (nbytes >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
    {
        *encoding_hint = dup_cstr("utf-16-le-bom");
        return;
    }
    if (nbytes >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
    {
        *encoding_hint = dup_cstr("utf-16-be-bom");
        return;
    }
    *encoding_hint = dup_cstr("unknown");
}

static int local_read(void* provider_ctx, const char* uri, IoReadResult* out, size_t max_read,
                      unsigned timeout_ms)
{
    (void)provider_ctx;

    if (!uri || !out)
        return BS_IO_ERR_INVALID_ARG;

    bs_io_read_result_init(out);

    const auto t0         = std::chrono::steady_clock::now();
    auto       elapsed_ms = [&t0]()
    {
        return static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - t0)
                                         .count());
    };

    auto fail_timeout = [&]()
    {
        out->status        = BS_IO_ERR_TIMEOUT;
        out->error_message = dup_cstr("read exceeded timeout_ms");
        return BS_IO_ERR_TIMEOUT;
    };

    std::string path;
    const int   path_rc = file_uri_to_path(uri, &path);
    if (path_rc != BS_IO_OK)
    {
        out->status        = path_rc;
        out->error_message = dup_cstr("invalid file URI");
        return path_rc;
    }

    if (timeout_ms == 0)
        return fail_timeout();

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        const char* hint = "file not found";
        errno            = 0;
        if (FILE* probe = std::fopen(path.c_str(), "rb"))
            std::fclose(probe);
        else if (errno == ENOSPC)
            hint = "enospc";
        else if (errno == EBUSY)
            hint = "ebusy";
        out->status        = BS_IO_ERR_NOT_FOUND;
        out->error_message = dup_cstr(hint);
        return BS_IO_ERR_NOT_FOUND;
    }

    if (timeout_ms > 0 && elapsed_ms() > timeout_ms)
        return fail_timeout();

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0)
    {
        out->status        = BS_IO_ERR_PROVIDER;
        out->error_message = dup_cstr("stat failed");
        return BS_IO_ERR_PROVIDER;
    }
    in.seekg(0, std::ios::beg);

    const size_t to_read =
        (static_cast<size_t>(size) > max_read) ? max_read : static_cast<size_t>(size);

    if (to_read > 0)
    {
        out->data = static_cast<uint8_t*>(std::malloc(to_read));
        if (!out->data)
        {
            out->status        = BS_IO_ERR_PROVIDER;
            out->error_message = dup_cstr("oom");
            return BS_IO_ERR_PROVIDER;
        }
        in.read(reinterpret_cast<char*>(out->data), static_cast<std::streamsize>(to_read));
    }

    if (!in && !in.eof())
    {
        bs_io_read_result_free(out);
        bs_io_read_result_init(out);
        out->status        = BS_IO_ERR_PROVIDER;
        out->error_message = dup_cstr("read failed");
        return BS_IO_ERR_PROVIDER;
    }

    if (timeout_ms > 0 && elapsed_ms() > timeout_ms)
    {
        bs_io_read_result_free(out);
        bs_io_read_result_init(out);
        return fail_timeout();
    }

    out->length     = to_read;
    out->truncated  = (static_cast<size_t>(size) > max_read) ? 1 : 0;
    out->source_uri = dup_cstr(uri);
    out->mime_hint  = dup_cstr("application/octet-stream");
    detect_bom_encoding(out->data, to_read, &out->encoding_hint);
    out->checksum = nullptr;
    out->status   = BS_IO_OK;
    return BS_IO_OK;
}

static int local_stat(void* provider_ctx, const char* uri, int64_t* out_size, int* out_exists)
{
    (void)provider_ctx;
    if (!uri || !out_size || !out_exists)
        return BS_IO_ERR_INVALID_ARG;

    std::string path;
    const int   path_rc = file_uri_to_path(uri, &path);
    if (path_rc != BS_IO_OK)
        return path_rc;

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        *out_exists = 0;
        *out_size   = 0;
        return BS_IO_OK;
    }
    *out_exists = 1;
    *out_size   = static_cast<int64_t>(in.tellg());
    return BS_IO_OK;
}

static void local_destroy(void* provider_ctx)
{
    auto* p = static_cast<LocalFileProvider*>(provider_ctx);
    delete p;
}

static const IoProviderOps kLocalOps = {
    BS_IO_PROVIDER_OPS_VERSION,
    local_read,
    local_stat,
    local_destroy,
};

LocalFileProvider* bs_adapter_io_local_provider_create(void)
{
    auto* p        = new LocalFileProvider();
    p->binding.ops = &kLocalOps;
    p->binding.ctx = p;
    return p;
}

void bs_adapter_io_local_provider_destroy(LocalFileProvider* provider)
{
    delete provider;
}

IoProviderBinding* bs_adapter_io_local_provider_binding(LocalFileProvider* provider)
{
    return provider ? &provider->binding : nullptr;
}
