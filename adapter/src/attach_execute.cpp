#include "bs/kernel/io/io.h"
#include "bs/kernel/ir/ir.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/Kernel.h"
#include "bs/kernel/runtime/kernel_pool.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_execute.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/parser/config_parse.h"

#include "attach_context_internal.h"

static AttachContext* resolve_active_ctx(AttachContext* ctx, int* skip_out)
{
    if (skip_out)
        *skip_out = 0;

    AttachContext* active = ctx;
    if (!active)
    {
        AttachActiveGuard guard;
        active = bs_adapter_attach_ctx_get_active();
    }
    if (!active)
    {
        if (skip_out)
            *skip_out = 1;
        return nullptr;
    }
    return active;
}

static int execute_instruction_list_kernel(Kernel* kernel, IRInstructionList* instructions,
                                           Report** report_out)
{
    if (!kernel || !instructions)
        return -1;

    const size_t instr_count = bs_ir_instruction_list_size(instructions);
    if (instr_count == 0)
        return -1;

    Report* last_report = nullptr;
    for (size_t i = 0; i < instr_count; ++i)
    {
        IRInstruction* const ir = bs_ir_instruction_list_get(instructions, i);
        if (!ir)
        {
            if (last_report)
                bs_report_destroy(last_report);
            return -1;
        }

        Report* exec_report = bs_kernel_execute(kernel, ir);
        if (!exec_report || bs_report_get_status(exec_report) != REPORT_STATUS_SUCCESS)
        {
            if (exec_report)
            {
                if (report_out)
                    *report_out = exec_report;
                else
                    bs_report_destroy(exec_report);
            }
            if (last_report)
                bs_report_destroy(last_report);
            return -1;
        }
        if (last_report)
            bs_report_destroy(last_report);
        last_report = exec_report;
    }

    if (report_out)
        *report_out = last_report;
    else if (last_report)
        bs_report_destroy(last_report);
    return 0;
}

static int execute_instruction_list_pool(BsKernelPool* pool, IRInstructionList* instructions,
                                         Report** report_out)
{
    if (!pool || !instructions)
        return -1;

    const size_t instr_count = bs_ir_instruction_list_size(instructions);
    if (instr_count == 0)
        return -1;

    Report* last_report = nullptr;
    for (size_t i = 0; i < instr_count; ++i)
    {
        IRInstruction* const ir = bs_ir_instruction_list_get(instructions, i);
        if (!ir)
        {
            if (last_report)
                bs_report_destroy(last_report);
            return -1;
        }

        Report* exec_report = nullptr;
        if (bs_kernel_pool_submit(pool, ir, &exec_report) != BS_KERNEL_POOL_OK || !exec_report ||
            bs_report_get_status(exec_report) != REPORT_STATUS_SUCCESS)
        {
            if (exec_report)
            {
                if (report_out)
                    *report_out = exec_report;
                else
                    bs_report_destroy(exec_report);
            }
            if (last_report)
                bs_report_destroy(last_report);
            return -1;
        }
        if (last_report)
            bs_report_destroy(last_report);
        last_report = exec_report;
    }

    if (report_out)
        *report_out = last_report;
    else if (last_report)
        bs_report_destroy(last_report);
    return 0;
}

int bs_adapter_attach_exec_parsed_ir(AttachContext* ctx, IRInstructionList* instructions,
                                     Report** report_out)
{
    if (report_out)
        *report_out = nullptr;

    int            skip   = 0;
    AttachContext* active = resolve_active_ctx(ctx, &skip);
    if (skip)
        return 0;
    if (!active)
        return -1;

    if (bs_adapter_attach_ctx_is_kernel_pool_warmed(active))
    {
        BsKernelPool* pool = bs_adapter_attach_ctx_kernel_pool(active);
        if (pool)
            return execute_instruction_list_pool(pool, instructions, report_out);
    }

    Kernel* kernel = bs_adapter_attach_ctx_kernel(active);
    if (!kernel)
        return 0;
    if (bs_kernel_get_state(kernel) != KERNEL_STATE_RUNNING)
        return -1;

    return execute_instruction_list_kernel(kernel, instructions, report_out);
}

int bs_adapter_attach_exec_gated_ir(AttachContext* ctx, const char* uri,
                                    const IoReadResult* read_result, Report** report_out)
{
    (void)uri;

    if (report_out)
        *report_out = nullptr;

    int            skip   = 0;
    AttachContext* active = resolve_active_ctx(ctx, &skip);
    if (skip)
        return 0;
    if (!active)
        return -1;

    BsConfigParseResult parsed{};
    const int           parse_rc =
        bs_adapter_attach_reload_parse_and_verify_bytes(read_result, &parsed, nullptr);
    if (parse_rc != BS_RELOAD_GATE_OK)
    {
        bs_adapter_parser_result_destroy(&parsed);
        return -1;
    }

    const int exec_rc   = bs_adapter_attach_exec_parsed_ir(active, parsed.instructions, report_out);
    parsed.instructions = nullptr;
    bs_adapter_parser_result_destroy(&parsed);
    return exec_rc;
}
