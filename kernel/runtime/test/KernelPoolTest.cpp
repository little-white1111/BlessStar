#include "bs/kernel/ir/ir.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/kernel_pool.h"

#include <cassert>
#include <cstdio>

#include <thread>
#include <vector>

static void test_pool_default_config_and_warmup()
{
    BsKernelPoolConfig config;
    bs_kernel_pool_config_init_default(&config);
    assert(config.steady_count == 3);
    assert(config.max_instances == 10);
    assert(config.fifo_wait_unbounded == 1);
    assert(config.per_batch_parallel_exec == 1);
    assert(config.slot_quarantine_on_failure == 0);

    BsKernelPool* pool = bs_kernel_pool_create(&config);
    assert(pool != nullptr);
    assert(bs_kernel_pool_warmup(pool) == BS_KERNEL_POOL_OK);

    BsKernelPoolStats stats{};
    assert(bs_kernel_pool_get_stats(pool, &stats) == BS_KERNEL_POOL_OK);
    assert(stats.steady_count == 3);
    assert(stats.max_instances == 10);
    assert(stats.total_slots == 3);
    assert(stats.dynamic_slots == 0);

    bs_kernel_pool_destroy(pool);
}

static void test_pool_submit_transfers_report_ownership()
{
    BsKernelPoolConfig config;
    bs_kernel_pool_config_init_default(&config);
    config.steady_count  = 1;
    config.max_instances = 2;

    BsKernelPool* pool = bs_kernel_pool_create(&config);
    assert(pool != nullptr);
    assert(bs_kernel_pool_warmup(pool) == BS_KERNEL_POOL_OK);

    IRInstruction* ir = bs_ir_instruction_create("noop", "pool-submit");
    assert(ir != nullptr);
    Report* report = nullptr;
    assert(bs_kernel_pool_submit(pool, ir, &report) == BS_KERNEL_POOL_OK);
    assert(report != nullptr);
    assert(bs_report_get_status(report) == REPORT_STATUS_SUCCESS);
    bs_report_destroy(report);
    bs_ir_instruction_destroy(ir);

    BsKernelPoolStats stats{};
    assert(bs_kernel_pool_get_stats(pool, &stats) == BS_KERNEL_POOL_OK);
    assert(stats.submitted_jobs == 1);
    assert(stats.completed_jobs == 1);
    assert(stats.failed_jobs == 0);

    bs_kernel_pool_destroy(pool);
}

static void test_pool_accepts_parallel_submitters()
{
    BsKernelPoolConfig config;
    bs_kernel_pool_config_init_default(&config);
    config.steady_count  = 2;
    config.max_instances = 4;

    BsKernelPool* pool = bs_kernel_pool_create(&config);
    assert(pool != nullptr);
    assert(bs_kernel_pool_warmup(pool) == BS_KERNEL_POOL_OK);

    constexpr int            kJobs = 12;
    std::vector<std::thread> threads;
    threads.reserve(kJobs);
    for (int i = 0; i < kJobs; ++i)
    {
        threads.emplace_back(
            [pool, i]()
            {
                IRInstruction* ir = bs_ir_instruction_create("noop", i % 2 == 0 ? "a" : "b");
                assert(ir != nullptr);
                Report* report = nullptr;
                assert(bs_kernel_pool_submit(pool, ir, &report) == BS_KERNEL_POOL_OK);
                assert(report != nullptr);
                bs_report_destroy(report);
                bs_ir_instruction_destroy(ir);
            });
    }
    for (auto& thread : threads)
        thread.join();

    BsKernelPoolStats stats{};
    assert(bs_kernel_pool_get_stats(pool, &stats) == BS_KERNEL_POOL_OK);
    assert(stats.submitted_jobs == kJobs);
    assert(stats.completed_jobs == kJobs);
    assert(stats.failed_jobs == 0);
    assert(stats.total_slots >= config.steady_count);
    assert(stats.total_slots <= config.max_instances);

    bs_kernel_pool_destroy(pool);
}

int main()
{
    test_pool_default_config_and_warmup();
    test_pool_submit_transfers_report_ownership();
    test_pool_accepts_parallel_submitters();
    std::printf("KernelPoolTest: PASS\n");
    return 0;
}
