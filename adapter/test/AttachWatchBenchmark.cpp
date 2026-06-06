#ifdef _WIN32
#define NOMINMAX
#include <psapi.h>
#include <windows.h>
#endif

#include "bs/adapter/persistence/attach_watch.h"

#include <chrono>
#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

static size_t current_rss_bytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)) != 0)
        return (size_t)pmc.WorkingSetSize;
    return 0;
#else
    return 0;
#endif
}

static double percentile_ns(std::vector<double>& v, double p)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const size_t idx = (size_t)((p / 100.0) * (double)(v.size() - 1));
    return v[idx];
}

int main()
{
    const int    tiers[]        = {64, 256, 500};
    const size_t ops_per_thread = 200;
    const char*  uri            = "file:///bench/watch";
    int          tok_metrics    = 0;
    int          tok_audit      = 0;
    const size_t rss_before_all = current_rss_bytes();

    bs_adapter_attach_persist_watch_metrics_reset();
    bs_adapter_attach_persist_watch_audit_reset();
    if (bs_adapter_attach_persist_watch_subscribe(bs_adapter_attach_persist_watch_metrics_on_event,
                                                  nullptr, &tok_metrics) != 0)
        return 1;
    if (bs_adapter_attach_persist_watch_subscribe(bs_adapter_attach_persist_watch_audit_on_event,
                                                  nullptr, &tok_audit) != 0)
        return 2;

    std::printf("=== Attach Watch Benchmark (Day15) ===\n");
    std::printf("tier,threads,total_events,p95_ns,p99_ns,rss_mb\n");

    std::atomic<uint64_t> epoch_seed{1};
    for (const int threads : tiers)
    {
        std::vector<std::thread> workers;
        workers.reserve((size_t)threads);
        std::vector<std::vector<double>> lat((size_t)threads);
        const size_t                     rss_before_tier = current_rss_bytes();

        for (int t = 0; t < threads; ++t)
        {
            workers.emplace_back(
                [&, t]()
                {
                    lat[(size_t)t].reserve(ops_per_thread);
                    for (size_t i = 0; i < ops_per_thread; ++i)
                    {
                        BsAttachWatchEvent ev{};
                        ev.epoch  = epoch_seed.fetch_add(1, std::memory_order_relaxed);
                        ev.uri    = uri;
                        ev.stage  = BS_ATTACH_WATCH_STAGE_WAL_FSYNC;
                        ev.result = BS_ATTACH_WATCH_RESULT_OK;

                        const auto st = std::chrono::high_resolution_clock::now();
                        (void)bs_adapter_attach_persist_watch_publish(&ev);
                        const auto ed = std::chrono::high_resolution_clock::now();
                        const auto ns =
                            std::chrono::duration_cast<std::chrono::nanoseconds>(ed - st).count();
                        lat[(size_t)t].push_back((double)ns);
                    }
                });
        }
        for (auto& th : workers)
            th.join();

        std::vector<double> merged;
        merged.reserve((size_t)threads * ops_per_thread);
        for (const auto& v : lat)
            merged.insert(merged.end(), v.begin(), v.end());

        const double p95            = percentile_ns(merged, 95.0);
        const double p99            = percentile_ns(merged, 99.0);
        const size_t rss_after_tier = current_rss_bytes();
        const size_t rss_delta =
            (rss_after_tier > rss_before_tier) ? (rss_after_tier - rss_before_tier) : 0;
        const double rss_mb = (double)rss_delta / (1024.0 * 1024.0);

        std::printf("%d,%d,%zu,%.2f,%.2f,%.2f\n", threads, threads, merged.size(), p95, p99,
                    rss_mb);
    }

    BsAttachWatchMetrics m{};
    BsAttachWatchAudit   a{};
    bs_adapter_attach_persist_watch_metrics_snapshot(&m);
    bs_adapter_attach_persist_watch_audit_snapshot(&a);
    const size_t rss_after_all = current_rss_bytes();
    const size_t rss_delta_all =
        (rss_after_all > rss_before_all) ? (rss_after_all - rss_before_all) : 0;
    std::printf("summary,total_events=%llu,fail_count=%llu,conservative=%llu,rss_total_mb=%.2f\n",
                (unsigned long long)m.total_events, (unsigned long long)m.fail_count,
                (unsigned long long)a.conservative_recover_count,
                (double)rss_delta_all / (1024.0 * 1024.0));

    bs_adapter_attach_persist_watch_unsubscribe(tok_audit);
    bs_adapter_attach_persist_watch_unsubscribe(tok_metrics);
    return 0;
}
