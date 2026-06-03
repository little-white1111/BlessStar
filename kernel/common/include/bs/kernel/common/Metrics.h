#ifndef BS_KERNEL_COMMON_METRICS_H
#define BS_KERNEL_COMMON_METRICS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Metrics counters use atomics where implemented; see Metrics.cpp.
 * Error semantics: Recording failures are silent drops in MVP.
 * Platform notes: Lightweight counters for benchmarks and diagnostics.
 */

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C"
{
#else
#include <stddef.h>
#include <stdint.h>
#endif

    typedef enum BsMetricType
    {
        BS_METRIC_TYPE_COUNTER   = 1,
        BS_METRIC_TYPE_GAUGE     = 2,
        BS_METRIC_TYPE_HISTOGRAM = 3,
        BS_METRIC_TYPE_SUMMARY   = 4
    } BsMetricType;

    typedef struct BsMetricLabel
    {
        const char*           key;
        const char*           value;
        struct BsMetricLabel* next;
    } BsMetricLabel;

    typedef struct BsMetric         BsMetric;
    typedef struct BsMetricRegistry BsMetricRegistry;

    BsMetricRegistry* bs_metric_registry_create(void);
    void              bs_metric_registry_destroy(BsMetricRegistry* registry);

    BsMetric* bs_metric_counter_create(BsMetricRegistry* registry, const char* name,
                                       const char* help);
    BsMetric* bs_metric_gauge_create(BsMetricRegistry* registry, const char* name,
                                     const char* help);

    void bs_metric_add_label(BsMetric* metric, const char* key, const char* value);
    void bs_metric_counter_inc(BsMetric* metric);
    void bs_metric_counter_add(BsMetric* metric, uint64_t value);
    void bs_metric_gauge_set(BsMetric* metric, double value);
    void bs_metric_gauge_add(BsMetric* metric, double value);

    uint64_t bs_metric_counter_get(const BsMetric* metric);
    double   bs_metric_gauge_get(const BsMetric* metric);

    void bs_metric_record_duration(BsMetric* metric, uint64_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif
