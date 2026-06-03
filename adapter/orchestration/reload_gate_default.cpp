#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/common/bs_status.h"

#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/requirement_filter.h"

#include <cstring>

int bs_adapter_attach_reload_parse_and_verify_bytes(const IoReadResult*  read_result,
                                                    BsConfigParseResult* out,
                                                    BsReloadGateDetail*  detail_out)
{
    if (!read_result || read_result->status != BS_IO_OK || !read_result->data ||
        read_result->length == 0)
        return BS_RELOAD_GATE_PARSE_FAIL;
    if (!out)
        return BS_RELOAD_GATE_PARSE_FAIL;

    const BsStatus st = bs_adapter_parser_parse_bytes(read_result->data, read_result->length, out);
    if (!bs_status_is_ok(st))
    {
        if (detail_out)
        {
            bs_safe_snprintf(detail_out->buf, sizeof(detail_out->buf),
                             "parse error at line %zu column %zu (domain=%d code=%d)",
                             out->error_line, out->error_column, bs_status_domain_id(st),
                             bs_status_code(st));
        }
        return BS_RELOAD_GATE_PARSE_FAIL;
    }

    const int gate_rc = bs_adapter_requirement_filter_verify_instructions(out->instructions,
                                                                          out->active_requirements);
    if (gate_rc != 0)
        return BS_RELOAD_GATE_IR_REJECT;
    return BS_RELOAD_GATE_OK;
}

int bs_adapter_attach_reload_default_path_gate(void* /*user_ctx*/, const char* /*uri*/,
                                               const IoReadResult* read_result,
                                               BsReloadGateDetail* detail_out)
{
    BsConfigParseResult parsed{};
    const int           rc =
        bs_adapter_attach_reload_parse_and_verify_bytes(read_result, &parsed, detail_out);
    bs_adapter_parser_result_destroy(&parsed);
    return rc;
}
