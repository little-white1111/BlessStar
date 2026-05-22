#include "bs/kernel/common/Metrics.h"

#include <cassert>
#include <cstdio>

#include <thread>
#include <vector>

static void test_Metrics_AllTypes()
{
    BsMetricRegistry* registry = bs_metric_registry_create();

    BsMetric* counter = bs_metric_counter_create(registry, "test_counter", "test counter");
    BsMetric* gauge   = bs_metric_gauge_create(registry, "test_gauge", "test gauge");

    assert(counter != nullptr);
    assert(gauge != nullptr);

    bs_metric_counter_inc(counter);
    bs_metric_counter_add(counter, 9);
    assert(bs_metric_counter_get(counter) == 10);

    bs_metric_gauge_set(gauge, 100.5);
    bs_metric_gauge_add(gauge, 20.5);
    assert(bs_metric_gauge_get(gauge) == 121.0);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_AllTypes: PASS\n");
}

static void test_Metrics_ThreadSafety()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric* counter = bs_metric_counter_create(registry, "thread_counter", "thread counter");

    const int                N = 10000;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; i++)
    {
        threads.emplace_back(
            [counter, N]()
            {
                for (int j = 0; j < N; j++)
                {
                    bs_metric_counter_inc(counter);
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    assert(bs_metric_counter_get(counter) == 10 * N);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_ThreadSafety: PASS\n");
}

static void test_Metrics_Labels()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric* counter = bs_metric_counter_create(registry, "labeled_counter", "labeled counter");

    bs_metric_add_label(counter, "module", "ir");
    bs_metric_add_label(counter, "type", "instruction");
    bs_metric_add_label(counter, "version", "1.0");

    bs_metric_counter_inc(counter);
    assert(bs_metric_counter_get(counter) == 1);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_Labels: PASS\n");
}

static void test_Metrics_Duration()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric* counter = bs_metric_counter_create(registry, "duration_counter", "duration counter");

    for (int i = 0; i < 100; i++)
    {
        bs_metric_record_duration(counter, i);
    }

    bs_metric_counter_inc(counter);
    assert(bs_metric_counter_get(counter) == 1);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_Duration: PASS\n");
}

static void test_Metrics_SpecialValues()
{
    BsMetric* metric = bs_metric_counter_create(nullptr, "test", "test");
    assert(metric == nullptr);

    bs_metric_counter_inc(nullptr);
    bs_metric_counter_add(nullptr, 10);
    bs_metric_gauge_set(nullptr, 100.0);
    bs_metric_gauge_add(nullptr, 10.0);
    bs_metric_add_label(nullptr, "key", "value");
    bs_metric_record_duration(nullptr, 100);

    assert(bs_metric_counter_get(nullptr) == 0);
    assert(bs_metric_gauge_get(nullptr) == 0.0);

    printf("test_Metrics_SpecialValues: PASS\n");
}

static void test_Metrics_LargeNumbers()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric* counter = bs_metric_counter_create(registry, "large_counter", "large counter");

    bs_metric_counter_add(counter, 1000000000ULL);
    bs_metric_counter_add(counter, 1000000000ULL);
    assert(bs_metric_counter_get(counter) == 2000000000ULL);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_LargeNumbers: PASS\n");
}

int main()
{
    printf("=== Metrics Comprehensive Tests ===\n");
    test_Metrics_AllTypes();
    test_Metrics_ThreadSafety();
    test_Metrics_Labels();
    test_Metrics_Duration();
    test_Metrics_SpecialValues();
    test_Metrics_LargeNumbers();
    printf("=== All Metrics Comprehensive Tests PASSED! ===\n");
    return 0;
}
