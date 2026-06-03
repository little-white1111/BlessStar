#ifndef BS_ADAPTER_ORCHESTRATION_RELOAD_GATE_DEFAULT_H
#define BS_ADAPTER_ORCHESTRATION_RELOAD_GATE_DEFAULT_H

/*
 * C-ST-7 contract block:
 * Thread safety: Gate runs on batch thread; must not re-enter attach store unsafely.
 * Error semantics: BS_RELOAD_GATE_* codes; parse failures vs IR reject distinguished.
 * Platform notes: parse_and_verify_bytes shared by gate and execute_gated_ir.
 *   set_default_gate enables single-parse cache on batch (XVII-KERNEL-4); standalone
 *   default_path_gate callback parses once and discards (non-batch callers).
 */

#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/parser/config_parse.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** Gate return codes for ReloadPathGateFn (M3: parse vs ir_gate reporting). */
    enum
    {
        BS_RELOAD_GATE_OK         = 0,
        BS_RELOAD_GATE_PARSE_FAIL = 1,
        BS_RELOAD_GATE_IR_REJECT  = 2
    };

    /**
     * Parse bytes and run requirement_filter (shared by default gate and reload execute).
     * On success, @p out owns parse result until bs_adapter_parser_result_destroy.
     * Returns BS_RELOAD_GATE_OK / BS_RELOAD_GATE_PARSE_FAIL / BS_RELOAD_GATE_IR_REJECT.
     */
    int bs_adapter_attach_reload_parse_and_verify_bytes(const IoReadResult*  read_result,
                                                        BsConfigParseResult* out,
                                                        BsReloadGateDetail*  detail_out);

    /** Default ReloadPathGateFn (parse+verify only; does not retain parse for execute). */
    int bs_adapter_attach_reload_default_path_gate(void* user_ctx, const char* uri,
                                                   const IoReadResult* read_result,
                                                   BsReloadGateDetail* detail_out);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ORCHESTRATION_RELOAD_GATE_DEFAULT_H */
