/* ── metrics_internal.h ────────────────────────────────────────────────
 * Shared internal metric struct definitions (C++ only).
 * Both Metrics.cpp and metrics_export.cpp include this to access
 * BsMetric / BsMetricRegistry internals without duplicating struct defs.
 * ──────────────────────────────────────────────────────────────────── */

#ifndef BS_METRICS_INTERNAL_H
#define BS_METRICS_INTERNAL_H

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

#include "bs/kernel/common/Metrics.h"

struct BsMetric
{
    BsMetricType   type;
    const char*    name;
    const char*    help;
    BsMetricLabel* labels;
    union
    {
        std::atomic<uint64_t> counter_value;
        std::atomic<double>   gauge_value;
    };
    std::atomic<uint64_t> duration_sum;
    std::atomic<uint32_t> duration_count;
};

struct BsMetricRegistry
{
    std::unordered_map<std::string, BsMetric*> metrics;
    std::mutex                                 mutex;
};

#endif /* BS_METRICS_INTERNAL_H */
