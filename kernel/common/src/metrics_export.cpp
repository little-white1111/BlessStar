/* ── metrics_export.cpp ───────────────────────────────────────────────
 * Prometheus text-format export for bs_metric_registry.
 * DAY38-10: 5 core counters/gauges → # HELP / # TYPE / metric lines
 * ──────────────────────────────────────────────────────────────────── */

#include "bs/kernel/common/Metrics.h"
#include "metrics_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/* ── C++ helpers (outside extern "C") ──────────────────────────────── */

static const char* metric_type_name(BsMetricType t)
{
    switch (t) {
        case BS_METRIC_TYPE_COUNTER:   return "counter";
        case BS_METRIC_TYPE_GAUGE:     return "gauge";
        case BS_METRIC_TYPE_HISTOGRAM: return "histogram";
        case BS_METRIC_TYPE_SUMMARY:   return "summary";
        default:                       return "untyped";
    }
}

static std::string format_labels(const BsMetricLabel* labels)
{
    if (!labels) return "";
    std::string result = "{";
    const BsMetricLabel* l = labels;
    bool first = true;
    while (l) {
        if (!first) result += ",";
        result += l->key ? l->key : "";
        result += "=\"";
        result += l->value ? l->value : "";
        result += "\"";
        first = false;
        l = l->next;
    }
    result += "}";
    return result;
}

/* ── Export single metric ──────────────────────────────────────────── */
static void export_one_metric(const BsMetric* metric,
                              std::string& out)
{
    if (!metric || !metric->name) return;

    std::string labels = format_labels(metric->labels);
    char line[1024];
    int w;

    /* HELP */
    w = snprintf(line, sizeof(line),
        "# HELP %s %s\n",
        metric->name,
        metric->help ? metric->help : "");
    if (w > 0) out += line;

    /* TYPE */
    w = snprintf(line, sizeof(line),
        "# TYPE %s %s\n",
        metric->name,
        metric_type_name(metric->type));
    if (w > 0) out += line;

    /* VALUE */
    switch (metric->type) {
    case BS_METRIC_TYPE_COUNTER:
        w = snprintf(line, sizeof(line),
            "%s%s %llu\n",
            metric->name, labels.c_str(),
            (unsigned long long)metric->counter_value.load());
        break;
    case BS_METRIC_TYPE_GAUGE: {
        double val = metric->gauge_value.load();
        w = snprintf(line, sizeof(line),
            "%s%s %g\n",
            metric->name, labels.c_str(), val);
        break;
    }
    case BS_METRIC_TYPE_HISTOGRAM:
    case BS_METRIC_TYPE_SUMMARY:
        w = snprintf(line, sizeof(line),
            "%s%s %g\n",
            metric->name, labels.c_str(),
            (double)metric->gauge_value.load());
        break;
    default:
        w = 0;
        break;
    }
    if (w > 0) out += line;
}

/* ── Public C ABI ──────────────────────────────────────────────────── */

extern "C" {

size_t bs_metrics_export_prometheus(BsMetricRegistry* registry, char** out_json)
{
    if (!registry || !out_json) return 0;

    std::lock_guard<std::mutex> lock(registry->mutex);
    std::string result;

    for (const auto& pair : registry->metrics) {
        export_one_metric(pair.second, result);
    }

    if (result.empty()) {
        *out_json = nullptr;
        return 0;
    }

    char* buf = (char*)malloc(result.size() + 1);
    if (!buf) return 0;
    memcpy(buf, result.c_str(), result.size() + 1);
    *out_json = buf;
    return result.size();
}

} /* extern "C" */
