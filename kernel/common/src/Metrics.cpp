#include "bs/kernel/common/Metrics.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

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

BsMetricRegistry* bs_metric_registry_create(void)
{
    return new BsMetricRegistry();
}

void bs_metric_registry_destroy(BsMetricRegistry* registry)
{
    if (!registry)
        return;
    for (auto& pair : registry->metrics)
    {
        BsMetric*      metric = pair.second;
        BsMetricLabel* label  = metric->labels;
        while (label)
        {
            BsMetricLabel* next = label->next;
            delete label;
            label = next;
        }
        metric->duration_count.~atomic();
        metric->duration_sum.~atomic();
        if (metric->type == BS_METRIC_TYPE_COUNTER)
        {
            metric->counter_value.~atomic();
        }
        else
        {
            metric->gauge_value.~atomic();
        }
        free(metric);
    }
    delete registry;
}

BsMetric* bs_metric_counter_create(BsMetricRegistry* registry, const char* name, const char* help)
{
    if (!registry || !name)
        return nullptr;

    std::lock_guard<std::mutex> lock(registry->mutex);
    std::string                 name_str(name);

    if (registry->metrics.count(name_str))
    {
        return registry->metrics[name_str];
    }

    BsMetric* metric = reinterpret_cast<BsMetric*>(malloc(sizeof(BsMetric)));
    if (!metric)
        return nullptr;

    metric->type   = BS_METRIC_TYPE_COUNTER;
    metric->name   = name;
    metric->help   = help;
    metric->labels = nullptr;
    new (&metric->counter_value) std::atomic<uint64_t>(0);
    new (&metric->duration_sum) std::atomic<uint64_t>(0);
    new (&metric->duration_count) std::atomic<uint32_t>(0);

    registry->metrics[name_str] = metric;
    return metric;
}

BsMetric* bs_metric_gauge_create(BsMetricRegistry* registry, const char* name, const char* help)
{
    if (!registry || !name)
        return nullptr;

    std::lock_guard<std::mutex> lock(registry->mutex);
    std::string                 name_str(name);

    if (registry->metrics.count(name_str))
    {
        return registry->metrics[name_str];
    }

    BsMetric* metric = reinterpret_cast<BsMetric*>(malloc(sizeof(BsMetric)));
    if (!metric)
        return nullptr;

    metric->type   = BS_METRIC_TYPE_GAUGE;
    metric->name   = name;
    metric->help   = help;
    metric->labels = nullptr;
    new (&metric->gauge_value) std::atomic<double>(0.0);
    new (&metric->duration_sum) std::atomic<uint64_t>(0);
    new (&metric->duration_count) std::atomic<uint32_t>(0);

    registry->metrics[name_str] = metric;
    return metric;
}

void bs_metric_add_label(BsMetric* metric, const char* key, const char* value)
{
    if (!metric || !key || !value)
        return;

    BsMetricLabel* label = new BsMetricLabel();
    label->key           = key;
    label->value         = value;
    label->next          = metric->labels;
    metric->labels       = label;
}

void bs_metric_counter_inc(BsMetric* metric)
{
    if (!metric)
        return;
    metric->counter_value.fetch_add(1);
}

void bs_metric_counter_add(BsMetric* metric, uint64_t value)
{
    if (!metric)
        return;
    metric->counter_value.fetch_add(value);
}

void bs_metric_gauge_set(BsMetric* metric, double value)
{
    if (!metric)
        return;
    metric->gauge_value.store(value);
}

void bs_metric_gauge_add(BsMetric* metric, double value)
{
    if (!metric)
        return;
    double old = metric->gauge_value.load();
    while (!metric->gauge_value.compare_exchange_weak(old, old + value))
        ;
}

uint64_t bs_metric_counter_get(const BsMetric* metric)
{
    if (!metric)
        return 0;
    return metric->counter_value.load();
}

double bs_metric_gauge_get(const BsMetric* metric)
{
    if (!metric)
        return 0.0;
    return metric->gauge_value.load();
}

void bs_metric_record_duration(BsMetric* metric, uint64_t duration_ms)
{
    if (!metric)
        return;
    metric->duration_sum.fetch_add(duration_ms);
    metric->duration_count.fetch_add(1);
}
