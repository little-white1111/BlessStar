#ifndef BS_KERNEL_COMMON_BS_STATUS_H
#define BS_KERNEL_COMMON_BS_STATUS_H

#include <stddef.h>
#include <stdint.h>

struct RegistryFacade;

#ifdef __cplusplus
extern "C"
{
#endif

/** Domain segment size for encoded failures (ERR-VII-2). */
#define BS_STATUS_DOMAIN_ENCODE_K 1000

    typedef int BsStatus;

#define BS_STATUS_OK 0

    typedef struct BsStatusCodeEntry
    {
        int         code;
        const char* name;
        uint32_t    flags;
    } BsStatusCodeEntry;

    typedef struct BsStatusDomainRegistration
    {
        const char*              domain_qname;
        const BsStatusCodeEntry* table;
        size_t                   table_len;
        uint16_t*                out_domain_id;
    } BsStatusDomainRegistration;

    BsStatus bs_status_make(uint16_t domain_id, int code);
    int      bs_status_domain_id(BsStatus status);
    int      bs_status_code(BsStatus status);
    int      bs_status_is_ok(BsStatus status);

    /**
     * Observation-only qualified name (ERR-VII-5), e.g. "io.TIMEOUT".
     * @return 0 ok, -1 buffer too small or unknown status
     */
    int bs_status_format(BsStatus status, struct RegistryFacade* facade, char* buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_COMMON_BS_STATUS_H */
