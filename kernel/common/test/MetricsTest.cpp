#include "bs/kernel/common/Metrics.h"

#include <cassert>
#include <cstdio>

static void test_Metrics_Registry()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    assert(registry != nullptr);
    bs_metric_registry_destroy(registry);
    printf("test_Metrics_Registry: PASS\n");
}

static void test_Metrics_Counter()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric*         counter  = bs_metric_counter_create(registry, "test_counter", "Test counter");
    assert(counter != nullptr);

    bs_metric_counter_inc(counter);
    assert(bs_metric_counter_get(counter) == 1);

    bs_metric_counter_add(counter, 9);
    assert(bs_metric_counter_get(counter) == 10);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_Counter: PASS\n");
}

static void test_Metrics_Gauge()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric*         gauge    = bs_metric_gauge_create(registry, "test_gauge", "Test gauge");
    assert(gauge != nullptr);

    bs_metric_gauge_set(gauge, 100.5);
    assert(bs_metric_gauge_get(gauge) == 100.5);

    bs_metric_gauge_add(gauge, 20.5);
    assert(bs_metric_gauge_get(gauge) == 121.0);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_Gauge: PASS\n");
}

static void test_Metrics_Labels()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric* counter = bs_metric_counter_create(registry, "test_labels", "Test with labels");
    assert(counter != nullptr);

    bs_metric_add_label(counter, "module", "ir");
    bs_metric_add_label(counter, "type", "instruction");

    bs_metric_counter_inc(counter);
    assert(bs_metric_counter_get(counter) == 1);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_Labels: PASS\n");
}

static void test_Metrics_Duration()
{
    BsMetricRegistry* registry = bs_metric_registry_create();
    BsMetric* counter = bs_metric_counter_create(registry, "test_duration", "Test duration");
    assert(counter != nullptr);

    bs_metric_record_duration(counter, 100);
    bs_metric_record_duration(counter, 200);
    bs_metric_record_duration(counter, 300);

    bs_metric_counter_inc(counter);
    assert(bs_metric_counter_get(counter) == 1);

    bs_metric_registry_destroy(registry);
    printf("test_Metrics_Duration: PASS\n");
}

static void test_Metrics_NullInput()
{
    BsMetric* metric = bs_metric_counter_create(nullptr, "test", "test");
    assert(metric == nullptr);

    bs_metric_counter_inc(nullptr);
    bs_metric_counter_add(nullptr, 10);
    bs_metric_gauge_set(nullptr, 100.0);
    bs_metric_gauge_add(nullptr, 10.0);
    bs_metric_add_label(nullptr, "key", "value");
    bs_metric_record_duration(nullptr, 100);

    uint64_t cnt = bs_metric_counter_get(nullptr);
    assert(cnt == 0);
    double g = bs_metric_gauge_get(nullptr);
    assert(g == 0.0);

    bs_metric_registry_destroy(nullptr);
    printf("test_Metrics_NullInput: PASS\n");
}

int main()
{
    printf("=== Metrics Tests ===\n");
    test_Metrics_Registry();
    test_Metrics_Counter();
    test_Metrics_Gauge();
    test_Metrics_Labels();
    test_Metrics_Duration();
    test_Metrics_NullInput();
    printf("=== All Metrics Tests PASSED! ===\n");
    return 0;
}
