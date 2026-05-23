#ifndef BS_ADAPTER_ORCHESTRATION_RELOAD_GATE_DEFAULT_H
#define BS_ADAPTER_ORCHESTRATION_RELOAD_GATE_DEFAULT_H

#include "bs/adapter/orchestration/reload_batch_controller.h"

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

    /** Install production default gate: bs_config_parse_bytes + verify_instructions. */
    void bs_reload_batch_controller_use_default_gate(ReloadBatchController* ctrl);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ORCHESTRATION_RELOAD_GATE_DEFAULT_H */
