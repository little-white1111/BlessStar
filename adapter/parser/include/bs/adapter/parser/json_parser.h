#ifndef BS_ADAPTER_PARSER_JSON_PARSER_H
#define BS_ADAPTER_PARSER_JSON_PARSER_H

#include "bs/adapter/parser/config_v1_ast.h"
#include "bs/kernel/common/bs_status.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Parse BlessStar Config JSON v1 bytes into @p out_ast (caller frees via config_v1_ast_destroy).
     * On failure sets @p error_line / @p error_column when non-null.
     */
    BsStatus json_parse_config_v1(const char* data, size_t len, ConfigV1Ast** out_ast,
                                  size_t* error_line, size_t* error_column);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_JSON_PARSER_H */
