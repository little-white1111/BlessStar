/**
 * Day19 profile presets (keep in sync with tools/scripts/perf/day19_profile.json).
 */

#pragma once

#include <cstdlib>
#include <cstring>

struct BsDay19Profile
{
    const char* name                      = "ci";
    int         duration_sec_max          = 25;
    double      day_wall_fraction         = 0.7;
    int         min_day_reloads           = 30;
    int         min_night_batches         = 5;
    int         path_pool_size            = 4;
    int         paths_per_batch           = 4;
    int         fixture_kb                = 100;
    int         rss_sample_interval_sec   = 5;
    double      rss_slope_mb_per_hour_max = 50.0;
    double      rss_delta_mb_max          = 200.0;
    double      outcome_ok_rate_min       = 0.99;
    int         disk_budget_mb            = 2048;
};

inline BsDay19Profile bs_day19_profile_ci()
{
    BsDay19Profile p{};
    p.name                      = "ci";
    p.duration_sec_max          = 25;
    p.day_wall_fraction         = 0.7;
    p.min_day_reloads           = 30;
    p.min_night_batches         = 5;
    p.path_pool_size            = 4;
    p.paths_per_batch           = 4;
    p.fixture_kb                = 100;
    p.rss_sample_interval_sec   = 5;
    p.rss_slope_mb_per_hour_max = 500.0; /* ci: slope not enforced in test */
    p.rss_delta_mb_max          = 200.0;
    p.outcome_ok_rate_min       = 0.99;
    return p;
}

inline BsDay19Profile bs_day19_profile_smoke()
{
    BsDay19Profile p{};
    p.name                      = "smoke";
    p.duration_sec_max          = 900;
    p.day_wall_fraction         = 0.7;
    p.min_day_reloads           = 1500;
    p.min_night_batches         = 30;
    p.path_pool_size            = 12;
    p.paths_per_batch           = 12;
    p.fixture_kb                = 100;
    p.rss_sample_interval_sec   = 60;
    p.rss_slope_mb_per_hour_max = 0.5;
    p.rss_delta_mb_max          = 50.0;
    p.outcome_ok_rate_min       = 0.995;
    return p;
}

inline BsDay19Profile bs_day19_profile_full()
{
    BsDay19Profile p            = bs_day19_profile_smoke();
    p.name                      = "full";
    p.duration_sec_max          = 259200;
    p.day_wall_fraction         = 0.5;
    p.min_day_reloads           = 8640;
    p.min_night_batches         = 144;
    p.rss_slope_mb_per_hour_max = 0.5;
    p.rss_delta_mb_max          = 50.0;
    p.outcome_ok_rate_min       = 0.999;
    return p;
}

inline BsDay19Profile bs_day19_profile_from_name(const char* name)
{
    if (!name || name[0] == '\0')
        return bs_day19_profile_ci();
    if (std::strcmp(name, "smoke") == 0)
        return bs_day19_profile_smoke();
    if (std::strcmp(name, "full") == 0)
        return bs_day19_profile_full();
    return bs_day19_profile_ci();
}

inline BsDay19Profile bs_day19_profile_from_argv_env(int argc, char** argv)
{
    const char* name = std::getenv("BS_DAY19_PROFILE");
    for (int i = 1; i < argc; ++i)
    {
        const char* arg      = argv[i];
        const char  prefix[] = "--profile=";
        if (std::strncmp(arg, prefix, sizeof(prefix) - 1) == 0)
            name = arg + sizeof(prefix) - 1;
    }
    return bs_day19_profile_from_name(name);
}
