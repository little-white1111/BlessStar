#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#include <cstddef>

#include "day19_rss_sampler.h"

size_t bs_day19_current_rss_bytes()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc)) != 0)
        return static_cast<size_t>(pmc.WorkingSetSize);
    return 0;
}

BsDay19RssMetrics bs_day19_current_memory_metrics()
{
    BsDay19RssMetrics          m{};
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc)) != 0)
    {
        m.working_set_bytes = static_cast<size_t>(pmc.WorkingSetSize);
        m.private_bytes     = static_cast<size_t>(pmc.PrivateUsage);
    }
    return m;
}

#endif /* _WIN32 */
