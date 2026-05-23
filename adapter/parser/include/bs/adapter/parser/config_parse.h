#ifndef BS_ADAPTER_PARSER_CONFIG_PARSE_H
#define BS_ADAPTER_PARSER_CONFIG_PARSE_H

#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/requirements.h"

#include "bs/adapter/parser/config_parse_status.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct BsConfigParseResult
    {
        IRInstructionList* instructions;
        IRRequirementList* active_requirements;
        size_t             error_line;
        size_t             error_column;
    } BsConfigParseResult;

    void bs_config_parse_result_destroy(BsConfigParseResult* result);

    BsStatus bs_config_parse_bytes(const uint8_t* data, size_t len, BsConfigParseResult* out);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_CONFIG_PARSE_H */
