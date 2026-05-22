#include "bs/kernel/io/io.h"

#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/registry/registry_facade.h"

#include <cstdlib>
#include <cstring>
#include <new>

struct IoFacade
{
    RegistryFacade* registry = nullptr;
};

IoFacade* bs_io_facade_create(RegistryFacade* registry)
{
    if (!registry)
        return nullptr;
    auto* f         = new (std::nothrow) IoFacade();
    if (!f)
        return nullptr;
    f->registry = registry;
    return f;
}

void bs_io_facade_destroy(IoFacade* facade)
{
    delete facade;
}

RegistryFacade* bs_io_facade_registry(IoFacade* facade)
{
    return facade ? facade->registry : nullptr;
}

void bs_io_read_result_init(IoReadResult* out)
{
    if (!out)
        return;
    std::memset(out, 0, sizeof(*out));
}

static char* io_strdup(const char* s)
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

void bs_io_read_result_free(IoReadResult* result)
{
    if (!result)
        return;
    std::free(result->data);
    std::free(result->source_uri);
    std::free(result->mime_hint);
    std::free(result->encoding_hint);
    std::free(result->checksum);
    std::free(result->error_message);
    bs_io_read_result_init(result);
}

static int parse_scheme(const char* uri, char* scheme, size_t scheme_size)
{
    if (!uri || !scheme || scheme_size == 0)
        return BS_IO_ERR_INVALID_ARG;
    const char* colon = std::strchr(uri, ':');
    if (!colon || colon == uri)
        return BS_IO_ERR_INVALID_URI;
    const size_t len = static_cast<size_t>(colon - uri);
    if (len + 1 > scheme_size)
        return BS_IO_ERR_INVALID_URI;
    std::memcpy(scheme, uri, len);
    scheme[len] = '\0';
    return BS_IO_OK;
}

int bs_io_provider_path_for_scheme(const char* scheme, char* out_path, size_t out_path_size)
{
    if (!scheme || !out_path || out_path_size < 16)
        return BS_IO_ERR_INVALID_ARG;

    if (std::strcmp(scheme, "file") == 0)
    {
        std::strncpy(out_path, "/adapter/io/local", out_path_size - 1);
        out_path[out_path_size - 1] = '\0';
        return BS_IO_OK;
    }
    if (std::strcmp(scheme, "db") == 0)
    {
        std::strncpy(out_path, "/adapter/io/db", out_path_size - 1);
        out_path[out_path_size - 1] = '\0';
        return BS_IO_OK;
    }
    if (std::strcmp(scheme, "remote") == 0)
    {
        std::strncpy(out_path, "/adapter/io/remote", out_path_size - 1);
        out_path[out_path_size - 1] = '\0';
        return BS_IO_OK;
    }
    return BS_IO_ERR_UNSUPPORTED_SCHEME;
}

static int resolve_provider(IoFacade* facade, const char* uri, IoProviderBinding** binding_out)
{
    char scheme[32];
    const int scheme_rc = parse_scheme(uri, scheme, sizeof(scheme));
    if (scheme_rc != BS_IO_OK)
        return scheme_rc;

    char provider_path[BS_REGISTRY_MAX_PATH];
    const int path_rc = bs_io_provider_path_for_scheme(scheme, provider_path, sizeof(provider_path));
    if (path_rc != BS_IO_OK)
        return path_rc;

    Binding b{};
    const int resolve_rc =
        bs_registry_facade_resolve(facade->registry, provider_path, &b);
    if (resolve_rc != BS_REGISTRY_OK)
        return BS_IO_ERR_REGISTRY;

    if (!b.impl)
        return BS_IO_ERR_NO_PROVIDER;

    *binding_out = static_cast<IoProviderBinding*>(b.impl);
    return BS_IO_OK;
}

int bs_io_facade_read(IoFacade* facade, const char* uri, IoReadResult* out)
{
    if (!facade || !uri || !out)
        return BS_IO_ERR_INVALID_ARG;

    if (bs_reentrancy_in_state_callback())
        return BS_IO_ERR_INVALID_ARG;

    bs_io_read_result_init(out);

    char scheme[32];
    const int scheme_rc = parse_scheme(uri, scheme, sizeof(scheme));
    if (scheme_rc != BS_IO_OK)
    {
        out->status         = scheme_rc;
        out->error_message  = io_strdup("invalid URI");
        return scheme_rc;
    }

    if (std::strcmp(scheme, "file") != 0)
    {
        out->status        = BS_IO_ERR_UNSUPPORTED_SCHEME;
        out->error_message = io_strdup("only file: scheme is formally supported in MVP");
        return BS_IO_ERR_UNSUPPORTED_SCHEME;
    }

    IoProviderBinding* binding = nullptr;
    const int bind_rc          = resolve_provider(facade, uri, &binding);
    if (bind_rc != BS_IO_OK)
    {
        out->status        = bind_rc;
        out->error_message = io_strdup("provider resolve failed");
        return bind_rc;
    }
    if (!binding->ops || !binding->ops->read)
    {
        out->status        = BS_IO_ERR_PROVIDER;
        out->error_message = io_strdup("provider missing read");
        return BS_IO_ERR_PROVIDER;
    }

    const int read_rc = binding->ops->read(binding->ctx, uri, out, BS_IO_MAX_READ_BYTES,
                                           BS_IO_READ_TIMEOUT_MS_DEFAULT);
    if (read_rc != BS_IO_OK && !out->error_message)
        out->error_message = io_strdup("provider read failed");
    return read_rc;
}

int bs_io_facade_stat(IoFacade* facade, const char* uri, int64_t* out_size, int* out_exists)
{
    if (!facade || !uri || !out_size || !out_exists)
        return BS_IO_ERR_INVALID_ARG;

    char scheme[32];
    const int scheme_rc = parse_scheme(uri, scheme, sizeof(scheme));
    if (scheme_rc != BS_IO_OK)
        return scheme_rc;
    if (std::strcmp(scheme, "file") != 0)
        return BS_IO_ERR_UNSUPPORTED_SCHEME;

    IoProviderBinding* binding = nullptr;
    const int bind_rc          = resolve_provider(facade, uri, &binding);
    if (bind_rc != BS_IO_OK)
        return bind_rc;
    if (!binding->ops || !binding->ops->stat)
        return BS_IO_ERR_PROVIDER;

    return binding->ops->stat(binding->ctx, uri, out_size, out_exists);
}
