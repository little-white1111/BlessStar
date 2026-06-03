/**
 * Process RSS sampling for Day19 memory / stress tests (XIX-MEM-11).
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <psapi.h>
#include <windows.h>
#endif

inline size_t bs_day19_current_rss_bytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc)) != 0)
        return static_cast<size_t>(pmc.WorkingSetSize);
    return 0;
#else
    std::ifstream status("/proc/self/status");
    if (!status)
        return 0;
    std::string line;
    while (std::getline(status, line))
    {
        if (line.rfind("VmRSS:", 0) == 0)
        {
            std::istringstream iss(line.substr(6));
            size_t             kb = 0;
            iss >> kb;
            return kb * 1024u;
        }
    }
    return 0;
#endif
}

struct BsDay19RssMetrics
{
    size_t working_set_bytes = 0;
    size_t private_bytes     = 0; /* Windows PrivateUsage; else same as WS */
};

inline BsDay19RssMetrics bs_day19_current_memory_metrics()
{
    BsDay19RssMetrics m{};
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc)) != 0)
    {
        m.working_set_bytes = static_cast<size_t>(pmc.WorkingSetSize);
        m.private_bytes     = static_cast<size_t>(pmc.PrivateUsage);
    }
#else
    m.working_set_bytes = bs_day19_current_rss_bytes();
    m.private_bytes     = m.working_set_bytes;
#endif
    return m;
}

struct BsDay19RssSample
{
    int64_t     timestamp_unix = 0;
    double      rss_mb         = 0.0; /* WorkingSet / VmRSS */
    double      private_mb     = 0.0; /* committed private bytes */
    const char* phase          = "";
    uint64_t    iter           = 0;
    int         outcome_ok     = 0;
};

inline void bs_day19_rss_sample_push(std::vector<BsDay19RssSample>& out, const char* phase,
                                     uint64_t iter, int outcome_ok)
{
    const auto              now = std::chrono::system_clock::now();
    const BsDay19RssMetrics mem = bs_day19_current_memory_metrics();
    BsDay19RssSample        s{};
    s.timestamp_unix = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    s.rss_mb     = static_cast<double>(mem.working_set_bytes) / (1024.0 * 1024.0);
    s.private_mb = static_cast<double>(mem.private_bytes) / (1024.0 * 1024.0);
    s.phase      = phase ? phase : "";
    s.iter       = iter;
    s.outcome_ok = outcome_ok;
    out.push_back(s);
}

/** Legacy end-minus-start slope (diagnostic only; do not use for gates). */
inline double bs_day19_rss_slope_endpoint_mb_per_hour(const std::vector<BsDay19RssSample>& samples)
{
    if (samples.size() < 2)
        return 0.0;
    const double t0   = static_cast<double>(samples.front().timestamp_unix);
    const double t1   = static_cast<double>(samples.back().timestamp_unix);
    const double dt_h = (t1 - t0) / 3600.0;
    if (dt_h <= 0.0)
        return 0.0;
    return (samples.back().rss_mb - samples.front().rss_mb) / dt_h;
}

/** Least-squares slope (MB/h) on private_mb; skips first `skip_first` samples (warmup). */
inline double
bs_day19_rss_slope_regression_mb_per_hour(const std::vector<BsDay19RssSample>& samples,
                                          size_t skip_first, bool use_private)
{
    if (samples.size() <= skip_first + 1)
        return 0.0;
    const int64_t t_base = samples[skip_first].timestamp_unix;
    double        n      = 0.0;
    double        sum_t  = 0.0;
    double        sum_r  = 0.0;
    double        sum_tt = 0.0;
    double        sum_tr = 0.0;
    for (size_t i = skip_first; i < samples.size(); ++i)
    {
        const double t = static_cast<double>(samples[i].timestamp_unix - t_base);
        const double r = use_private ? samples[i].private_mb : samples[i].rss_mb;
        n += 1.0;
        sum_t += t;
        sum_r += r;
        sum_tt += t * t;
        sum_tr += t * r;
    }
    const double denom = n * sum_tt - sum_t * sum_t;
    if (denom <= 0.0)
        return 0.0;
    const double b_per_sec = (n * sum_tr - sum_t * sum_r) / denom;
    return b_per_sec * 3600.0;
}

/**
 * XIX-MEM-10 style: baseline = min/avg of first window; peak = max of last window (private_mb).
 * `window_samples` = number of points per window (e.g. 10 for 10min @ 60s).
 */
inline double bs_day19_rss_delta_windowed_mb(const std::vector<BsDay19RssSample>& samples,
                                             size_t skip_first, size_t window_samples,
                                             bool use_private)
{
    if (samples.size() <= skip_first)
        return 0.0;
    const size_t n = samples.size() - skip_first;
    if (n == 0)
        return 0.0;
    const size_t win = window_samples > 0 ? window_samples : 1;
    const size_t w   = win < n ? win : n;

    auto metric = [use_private](const BsDay19RssSample& s)
    { return use_private ? s.private_mb : s.rss_mb; };

    double baseline = metric(samples[skip_first]);
    for (size_t i = skip_first; i < skip_first + w && i < samples.size(); ++i)
        baseline = (metric(samples[i]) < baseline) ? metric(samples[i]) : baseline;

    double       peak       = metric(samples.back());
    const size_t tail_start = samples.size() > w ? samples.size() - w : skip_first;
    for (size_t i = tail_start; i < samples.size(); ++i)
        peak = (metric(samples[i]) > peak) ? metric(samples[i]) : peak;

    return peak - baseline;
}

inline double bs_day19_rss_delta_mb(const std::vector<BsDay19RssSample>& samples)
{
    return bs_day19_rss_delta_windowed_mb(samples, 0, samples.size(), false);
}

inline void bs_day19_rss_print_samples(const std::vector<BsDay19RssSample>& samples, FILE* out)
{
    if (!out)
        out = stdout;
    std::fprintf(out, "rss_diag,timestamp,ws_mb,private_mb,phase,iter,ok\n");
    for (const auto& s : samples)
    {
        std::fprintf(out, "rss_diag,%lld,%.3f,%.3f,%s,%llu,%d\n",
                     static_cast<long long>(s.timestamp_unix), s.rss_mb, s.private_mb,
                     s.phase ? s.phase : "", static_cast<unsigned long long>(s.iter), s.outcome_ok);
    }
}
