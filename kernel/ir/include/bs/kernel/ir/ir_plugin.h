#ifndef BS_KERNEL_IR_IR_PLUGIN_H
#define BS_KERNEL_IR_IR_PLUGIN_H

/*
 * C-ST-7 contract block:
 * Thread safety: Plugin IR hooks must not call back into gate while holding locks.
 * Error semantics: See bs_status in ir_plugin.cpp for plugin-specific codes.
 * Platform notes: Bridges static plugin registration to IR requirement checks.
 */

#include "bs/kernel/ir/ir.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Optional visitor on IR instructions after translation, before gate (checkpoint 3: plugin on
     * IR path). Must not introduce types not covered by the active requirement list (enforced at
     * gate). Return 0 on success, non-zero to abort pipeline with contract-style failure.
     */
    typedef int (*BsIrInstructionVisitor)(IRInstruction* inst, void* user_data);

    void bs_ir_plugin_register_visitor(BsIrInstructionVisitor visitor, void* user_data);
    void bs_ir_plugin_unregister_visitor(BsIrInstructionVisitor visitor);
    int  bs_ir_plugin_apply_visitors(IRInstructionList* list);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_IR_IR_PLUGIN_H */
