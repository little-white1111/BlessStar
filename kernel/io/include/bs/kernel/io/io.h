#ifndef BS_KERNEL_IO_IO_H
#define BS_KERNEL_IO_IO_H

#include <stddef.h>
#include <stdint.h>

struct RegistryFacade;

#ifdef __cplusplus
extern "C"
{
#endif

    /** Max bytes per single read (4 MiB). */
#define BS_IO_MAX_READ_BYTES (4u * 1024u * 1024u)

    /** Default read timeout in milliseconds. */
#define BS_IO_READ_TIMEOUT_MS_DEFAULT 30000u

#define BS_IO_PROVIDER_OPS_VERSION 1

    typedef enum IoStatus
    {
        BS_IO_OK                        = 0,
        BS_IO_ERR_INVALID_URI           = -1,
        BS_IO_ERR_UNSUPPORTED_SCHEME    = -2,
        BS_IO_ERR_PROVIDER              = -3,
        BS_IO_ERR_READ_LIMIT            = -4,
        BS_IO_ERR_TIMEOUT               = -5,
        BS_IO_ERR_NOT_FOUND             = -6,
        BS_IO_ERR_INVALID_ARG           = -7,
        BS_IO_ERR_REGISTRY              = -8,
        BS_IO_ERR_NO_PROVIDER           = -9
    } IoStatus;

    typedef struct IoReadResult
    {
        int     status;
        uint8_t* data;
        size_t  length;
        int     truncated;
        char*   source_uri;
        char*   mime_hint;
        char*   encoding_hint;
        char*   checksum;
        char*   error_message;
    } IoReadResult;

    /**
     * Provider C ABI (IO-II-2). watch is intentionally absent in MVP (XV-IO-01 deferred).
     */
    typedef struct IoProviderOps
    {
        int version;
        int (*read)(void* provider_ctx, const char* uri, IoReadResult* out, size_t max_read,
                    unsigned timeout_ms);
        int (*stat)(void* provider_ctx, const char* uri, int64_t* out_size, int* out_exists);
        void (*destroy)(void* provider_ctx);
    } IoProviderOps;

    /** Stored as Binding.impl in Registry. */
    typedef struct IoProviderBinding
    {
        const IoProviderOps* ops;
        void*                ctx;
    } IoProviderBinding;

    typedef struct IoFacade IoFacade;

    IoFacade* bs_io_facade_create(struct RegistryFacade* registry);
    void      bs_io_facade_destroy(IoFacade* facade);

    struct RegistryFacade* bs_io_facade_registry(IoFacade* facade);

    /**
     * Single read via Registry resolve -> Provider (IO-II-3, R-II-5).
     * Only file: is formally supported in MVP; db:/remote: resolve to stubs.
     */
    int bs_io_facade_read(IoFacade* facade, const char* uri, IoReadResult* out);

    int bs_io_facade_stat(IoFacade* facade, const char* uri, int64_t* out_size, int* out_exists);

    void bs_io_read_result_init(IoReadResult* out);
    void bs_io_read_result_free(IoReadResult* result);

    /** Map URI scheme to canonical provider path (/adapter/io/...). */
    int bs_io_provider_path_for_scheme(const char* scheme, char* out_path, size_t out_path_size);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_IO_IO_H */
