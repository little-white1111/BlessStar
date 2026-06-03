#ifndef BS_ADAPTER_ATTACH_EXECUTE_H
#define BS_ADAPTER_ATTACH_EXECUTE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Same as active AttachContext / reload driver thread.
 * Error semantics: 0 success; -1 execute/parse failure (Report FAILED when report_out set).
 *   No active ctx or no Kernel: 0 (skip ir_execute). Kernel present but not RUNNING: -1.
 * Platform notes: XVII-KERNEL-4/8 facade; orchestration must not include Kernel.h.
 *   execute_parsed_ir: no re-parse (default gate cache). execute_gated_ir: parse+verify then run.
 */

struct AttachContext;
struct Report;
struct IoReadResult;
struct IRInstructionList;

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Parse gated bytes and run default Kernel pipeline (XVII-KERNEL-4).
     * Returns 0 on success; -1 on failure.
     * No active ctx, or legacy shell without Kernel: returns 0 (skip ir_execute).
     * Active ctx with Kernel but not RUNNING (freeze not done): returns -1.
     */
    int bs_adapter_attach_exec_gated_ir(AttachContext* ctx, const char* uri,
                                        const IoReadResult* read_result, Report** report_out);

    /** Run Kernel pipeline on already gated IR list (no re-parse; used after default gate cache).
     */
    int bs_adapter_attach_exec_parsed_ir(AttachContext* ctx, IRInstructionList* instructions,
                                         Report** report_out);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_EXECUTE_H */
