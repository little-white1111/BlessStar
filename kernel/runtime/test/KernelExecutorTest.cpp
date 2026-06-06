#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/Stage.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/Kernel.h"

#include <cassert>
#include <chrono>
#include <cstdio>

#include <atomic>
#include <thread>
#include <vector>

struct SerialStageContext
{
    std::atomic<int> active{0};
    std::atomic<int> calls{0};
    std::atomic<int> entered{0};
};

static int serial_stage_execute(Stage* stage, const IRInstruction* input, IRInstruction** output)
{
    assert(stage != nullptr);
    assert(input != nullptr);
    auto* ctx = static_cast<SerialStageContext*>(stage->context);
    assert(ctx != nullptr);
    const int previous = ctx->active.fetch_add(1);
    assert(previous == 0);
    ctx->entered.store(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ctx->calls.fetch_add(1);
    ctx->active.fetch_sub(1);
    if (output)
        *output = nullptr;
    return 0;
}

static void attach_default_pipeline(Kernel* kernel, Pipeline** out_pipeline,
                                    SerialStageContext* ctx)
{
    Pipeline* pipeline = bs_pipeline_create();
    assert(pipeline != nullptr);
    Stage stage{};
    stage.name    = "serial";
    stage.execute = serial_stage_execute;
    stage.context = ctx;
    assert(bs_pipeline_add_stage(pipeline, &stage) == 0);
    assert(bs_kernel_register_pipeline(kernel, "default", pipeline) == 0);
    *out_pipeline = pipeline;
}

static void test_ordered_worker_serializes_concurrent_execute()
{
    Kernel* kernel = bs_kernel_create(nullptr);
    assert(kernel != nullptr);
    SerialStageContext ctx;
    Pipeline*          pipeline = nullptr;
    attach_default_pipeline(kernel, &pipeline, &ctx);
    assert(bs_kernel_start(kernel) == 0);

    constexpr int            kJobs = 8;
    std::vector<std::thread> threads;
    threads.reserve(kJobs);
    for (int i = 0; i < kJobs; ++i)
    {
        threads.emplace_back(
            [kernel, i]()
            {
                IRInstruction* ir = bs_ir_instruction_create("noop", i % 2 == 0 ? "even" : "odd");
                assert(ir != nullptr);
                Report* report = bs_kernel_execute(kernel, ir);
                assert(report != nullptr);
                assert(bs_report_get_status(report) == REPORT_STATUS_SUCCESS);
                bs_report_destroy(report);
                bs_ir_instruction_destroy(ir);
            });
    }
    for (auto& thread : threads)
        thread.join();

    assert(ctx.calls.load() == kJobs);
    assert(bs_kernel_stop(kernel) == 0);
    assert(bs_kernel_unregister_pipeline(kernel, "default") == 0);
    bs_pipeline_destroy(pipeline);
    bs_kernel_destroy(kernel);
}

static void test_stop_drains_running_job()
{
    Kernel* kernel = bs_kernel_create(nullptr);
    assert(kernel != nullptr);
    SerialStageContext ctx;
    Pipeline*          pipeline = nullptr;
    attach_default_pipeline(kernel, &pipeline, &ctx);
    assert(bs_kernel_start(kernel) == 0);

    std::thread worker(
        [kernel]()
        {
            IRInstruction* ir = bs_ir_instruction_create("noop", "stop-drain");
            assert(ir != nullptr);
            Report* report = bs_kernel_execute(kernel, ir);
            assert(report != nullptr);
            bs_report_destroy(report);
            bs_ir_instruction_destroy(ir);
        });
    while (ctx.entered.load() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(bs_kernel_stop(kernel) == 0);
    worker.join();
    assert(ctx.calls.load() == 1);

    assert(bs_kernel_unregister_pipeline(kernel, "default") == 0);
    bs_pipeline_destroy(pipeline);
    bs_kernel_destroy(kernel);
}

int main()
{
    test_ordered_worker_serializes_concurrent_execute();
    test_stop_drains_running_job();
    std::printf("KernelExecutorTest: PASS\n");
    return 0;
}
