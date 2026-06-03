#ifndef BS_ADAPTER_PARSER_CONFIG_PARSE_STATUS_H
#define BS_ADAPTER_PARSER_CONFIG_PARSE_STATUS_H

/*
 * C-ST-7 contract block:
 * Thread safety: domain id set once at init; thereafter read-only mapping.
 * Error semantics: bs_status_from_config_parse maps parse_err_code to BsStatus.
 * Platform notes: N/A.
 */

#include "bs/kernel/common/bs_status.h"

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        BS_CONFIG_PARSE_ERR_INVALID_ARG = 1,
        BS_CONFIG_PARSE_ERR_LEX         = 2,
        BS_CONFIG_PARSE_ERR_PARSE       = 3,
        BS_CONFIG_PARSE_ERR_SCHEMA      = 4,
        BS_CONFIG_PARSE_ERR_KERNEL_VER  = 5,
        BS_CONFIG_PARSE_ERR_ADAPTER_VER = 6,
        BS_CONFIG_PARSE_ERR_OOM         = 7
    };

    void     bs_adapter_parser_status_set_domain_id(uint16_t domain_id);
    BsStatus bs_status_from_config_parse(int parse_err_code);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_CONFIG_PARSE_STATUS_H */
