#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/Stage.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/Kernel.h"

#include <cassert>
#include <cstdio>

static int pass_stage_execute(Stage* /*stage*/, const IRInstruction* input, IRInstruction** output)
{
    if (!input)
        return -1;
    if (output)
        *output = nullptr;
    return 0;
}

static void test_kernel_execute_default_pipeline()
{
    Kernel* kernel = bs_kernel_create(nullptr);
    assert(kernel != nullptr);

    Pipeline* pipeline = bs_pipeline_create();
    assert(pipeline != nullptr);

    Stage pass{};
    pass.name    = "pass";
    pass.execute = pass_stage_execute;
    assert(bs_pipeline_add_stage(pipeline, &pass) == 0);
    assert(bs_kernel_register_pipeline(kernel, "default", pipeline) == 0);
    assert(bs_kernel_start(kernel) == 0);

    IRInstruction* ir = bs_ir_instruction_create("noop", "t8");
    assert(ir != nullptr);

    Report* report = bs_kernel_execute(kernel, ir);
    assert(report != nullptr);
    assert(bs_report_get_status(report) == REPORT_STATUS_SUCCESS);
    bs_report_destroy(report);
    bs_ir_instruction_destroy(ir);

    assert(bs_kernel_stop(kernel) == 0);
    bs_kernel_unregister_pipeline(kernel, "default");
    bs_pipeline_destroy(pipeline);
    bs_kernel_destroy(kernel);
}

static void test_kernel_async_drain()
{
    Kernel* kernel = bs_kernel_create(nullptr);
    assert(kernel != nullptr);

    Pipeline* pipeline = bs_pipeline_create();
    Stage     pass{};
    pass.name    = "pass";
    pass.execute = pass_stage_execute;
    assert(bs_pipeline_add_stage(pipeline, &pass) == 0);
    assert(bs_kernel_register_pipeline(kernel, "default", pipeline) == 0);
    assert(bs_kernel_start(kernel) == 0);

    IRInstruction* ir = bs_ir_instruction_create("async", "job");
    assert(ir != nullptr);
    assert(bs_kernel_execute_async(kernel, ir) == 0);
    assert(bs_kernel_drain_async_queue(kernel) == 1);

    bs_ir_instruction_destroy(ir);
    assert(bs_kernel_stop(kernel) == 0);
    bs_kernel_unregister_pipeline(kernel, "default");
    bs_pipeline_destroy(pipeline);
    bs_kernel_destroy(kernel);
}

int main()
{
    test_kernel_execute_default_pipeline();
    test_kernel_async_drain();
    std::printf("KernelRuntimeTest: PASS\n");
    return 0;
}
