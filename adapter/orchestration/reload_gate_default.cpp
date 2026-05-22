#include "bs/adapter/orchestration/reload_gate_default.h"

#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/requirement_filter.h"
#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/common/bs_status.h"

#include <cstring>

static int default_gate_fn(void* /*user_ctx*/, const char* /*uri*/,
                           const IoReadResult* read_result, BsReloadGateDetail* detail_out)
{
    if (!read_result || read_result->status != BS_IO_OK || !read_result->data ||
        read_result->length == 0)
        return BS_RELOAD_GATE_PARSE_FAIL;

    BsConfigParseResult parsed{};
    const BsStatus      st =
        bs_config_parse_bytes(read_result->data, read_result->length, &parsed);
    if (!bs_status_is_ok(st))
    {
        if (detail_out)
        {
            bs_safe_snprintf(detail_out->buf, sizeof(detail_out->buf),
                            "parse error at line %zu column %zu (domain=%d code=%d)",
                            parsed.error_line, parsed.error_column, bs_status_domain_id(st),
                            bs_status_code(st));
        }
        return BS_RELOAD_GATE_PARSE_FAIL;
    }

    const int gate_rc = bs_adapter_requirement_filter_verify_instructions(
        parsed.instructions, parsed.active_requirements);
    bs_config_parse_result_destroy(&parsed);

    if (gate_rc != 0)
        return BS_RELOAD_GATE_IR_REJECT;
    return BS_RELOAD_GATE_OK;
}

void bs_reload_batch_controller_use_default_gate(ReloadBatchController* ctrl)
{
    if (!ctrl)
        return;
    bs_reload_batch_controller_set_gate_fn(ctrl, default_gate_fn, nullptr);
}
