#ifndef BS_KERNEL_IR_IR_GATE_H
#define BS_KERNEL_IR_IR_GATE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Gate evaluation is stateless aside from optional user context pointer.
 * Error semantics: Reject/accept encoded as BsStatus-compatible ints in implementation.
 * Platform notes: Placeholder IR gate hooks for reload orchestration integration.
 */

#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/requirements.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * IR gate (checkpoint 2): any instruction whose `type` is not listed in `requirements` ->
     * failure.
     * @return 0 success, -1 null/invalid args, 1 contract violation (unknown / supershape type)
     */
    int bs_ir_gate_verify_instructions(const IRInstructionList* list,
                                       const IRRequirementList* requirements);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_IR_IR_GATE_H */
