/**
 * T19.4 / T19.5: 72h-RP stress harness (PER_PATH day + PER_BATCH night).
 * Default profile "ci" for CTest; use --profile=smoke|full or BS_DAY19_PROFILE.
 */

#include "bs/adapter/attach_context.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_with_report.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <filesystem>
#include <string>
#include <vector>

#include "support/attach_test_fixture.h"
#include "support/day12_attach_fixture.h"
#include "support/day19_fixture_gen.h"
#include "support/day19_profile.h"
#include "support/day19_rss_sampler.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

static int write_path_pool(const fs::path& work, int count, size_t fixture_bytes,
                           std::vector<std::string>* uris_out)
{
    uris_out->clear();
    const std::string json = bs_day19_make_valid_v1_json(fixture_bytes);
    for (int i = 0; i < count; ++i)
    {
        const fs::path f = work / ("path-" + std::to_string(i) + ".json");
        if (!bs_test_write_binary_file(f, json.data(), json.size()))
            return -1;
        uris_out->push_back(bs_test_path_to_file_uri(f));
    }
    return 0;
}

static uint64_t dir_size_bytes(const fs::path& root)
{
    uint64_t        total = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (it->is_regular_file(ec))
            total += static_cast<uint64_t>(it->file_size(ec));
    }
    return total;
}

int main(int argc, char** argv)
{
    const BsDay19Profile profile = bs_day19_profile_from_argv_env(argc, argv);
    std::printf("Day19Stress profile=%s duration_max=%d day_reload_min=%d night_batch_min=%d\n",
                profile.name, profile.duration_sec_max, profile.min_day_reloads,
                profile.min_night_batches);

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_day19_stress"));
    const fs::path&          work = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_attach_context_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);
    bs_attach_context_set_active(fix.ctx);

    std::vector<std::string> uris;
    const size_t             fixture_bytes = static_cast<size_t>(profile.fixture_kb) * 1024u;
    BS_TEST_REQUIRE("paths",
                    write_path_pool(work, profile.path_pool_size, fixture_bytes, &uris) == 0);

    const fs::path                manifest_path = work / "manifest.bs";
    std::vector<BsDay19RssSample> samples;

    const auto t0          = std::chrono::steady_clock::now();
    auto       last_sample = t0;

    int  day_ok      = 0;
    int  day_total   = 0;
    int  night_ok    = 0;
    int  night_total = 0;
    int  path_i      = 0;
    bool steady_mark = false;

    auto elapsed_sec = [&]()
    {
        return static_cast<int>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0)
                .count());
    };

    auto maybe_sample = [&](const char* phase, uint64_t iter, int ok)
    {
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_sample).count() >=
            profile.rss_sample_interval_sec)
        {
            bs_day19_rss_sample_push(samples, phase, iter, ok);
            last_sample = now;
        }
    };

    while (true)
    {
        const int elapsed = elapsed_sec();
        if (elapsed >= profile.duration_sec_max && day_total >= profile.min_day_reloads &&
            night_total >= profile.min_night_batches)
            break;

        const int  day_limit  = static_cast<int>(static_cast<double>(profile.duration_sec_max) *
                                                 profile.day_wall_fraction);
        const bool need_day   = day_total < profile.min_day_reloads;
        const bool need_night = night_total < profile.min_night_batches;
        const bool day_phase  = need_day && (elapsed < day_limit || !need_night);

        if (day_phase)
        {
            ReloadBatchController* ctrl = bs_reload_batch_controller_create(16);
            BS_TEST_REQUIRE("day-ctrl", ctrl != nullptr);
            bs_reload_batch_controller_set_read_fn(ctrl, facade_read_fn, &fix);
            bs_reload_batch_controller_use_default_gate(ctrl);
            bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
            bs_reload_batch_controller_set_manifest_path(ctrl, manifest_path.string().c_str());

            const std::string& uri = uris[static_cast<size_t>(path_i % uris.size())];
            path_i++;
            ++day_total;
            const int add_rc = bs_reload_batch_add_path(ctrl, uri.c_str());
            const int run_rc = (add_rc == 0) ? bs_reload_batch_run(ctrl) : -1;
            const int ok = (run_rc == 0 && bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK) ? 1 : 0;
            day_ok += ok;
            if (!steady_mark && day_total >= 30)
            {
                bs_day19_rss_sample_push(samples, "steady_mark", static_cast<uint64_t>(day_total),
                                         ok);
                steady_mark = true;
            }
            maybe_sample("day", static_cast<uint64_t>(day_total), ok);
            if (day_total % 50 == 0)
                bs_day19_rss_sample_push(samples, "day_tick", static_cast<uint64_t>(day_total), ok);
            bs_reload_batch_controller_destroy(ctrl);
            continue;
        }
        if (need_night)
        {
            ReloadBatchController* ctrl = bs_reload_batch_controller_create(32);
            BS_TEST_REQUIRE("night-ctrl", ctrl != nullptr);
            bs_reload_batch_controller_set_read_fn(ctrl, facade_read_fn, &fix);
            bs_reload_batch_controller_use_default_gate(ctrl);
            bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_BATCH);
            bs_reload_batch_controller_set_manifest_path(ctrl, manifest_path.string().c_str());

            const int n = profile.paths_per_batch < profile.path_pool_size ? profile.paths_per_batch
                                                                           : profile.path_pool_size;
            int       add_fail = 0;
            for (int p = 0; p < n; ++p)
            {
                if (bs_reload_batch_add_path(ctrl, uris[static_cast<size_t>(p)].c_str()) != 0)
                    add_fail = 1;
            }
            ++night_total;
            const int run_rc = add_fail ? -1 : bs_reload_batch_run(ctrl);
            const int ok = (run_rc == 0 && bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK) ? 1 : 0;
            night_ok += ok;
            maybe_sample("night", static_cast<uint64_t>(night_total), ok);
            bs_day19_rss_sample_push(samples, "night_tick", static_cast<uint64_t>(night_total), ok);
            bs_reload_batch_controller_destroy(ctrl);
            continue;
        }
        if (elapsed >= profile.duration_sec_max)
            break;
    }

    bs_day19_rss_sample_push(samples, "pre_teardown", static_cast<uint64_t>(day_total), 1);

    size_t warmup_skip = (std::strcmp(profile.name, "ci") == 0) ? 2u : 5u;
    if (warmup_skip + 3 > samples.size())
        warmup_skip = samples.size() > 3 ? samples.size() - 3 : 0;

    const size_t window_samples = static_cast<size_t>(
        600 / (profile.rss_sample_interval_sec > 0 ? profile.rss_sample_interval_sec : 60));
    const size_t win = window_samples < (samples.size() - warmup_skip)
                           ? window_samples
                           : (samples.size() - warmup_skip);

    const double slope_endpoint_ws = bs_day19_rss_slope_endpoint_mb_per_hour(samples);
    double       slope_reg_ws =
        bs_day19_rss_slope_regression_mb_per_hour(samples, warmup_skip, false /* use_private */);
    double delta_ws   = bs_day19_rss_delta_windowed_mb(samples, warmup_skip, win, false);
    double delta_priv = bs_day19_rss_delta_windowed_mb(samples, warmup_skip, win, true);

    if (samples.size() < 8)
    {
        /* Too few points for regression / 10min windows (short ci). */
        slope_reg_ws = 0.0;
        double min_p = samples.empty() ? 0.0 : samples.front().private_mb;
        double max_p = min_p;
        for (const auto& s : samples)
        {
            min_p = (s.private_mb < min_p) ? s.private_mb : min_p;
            max_p = (s.private_mb > max_p) ? s.private_mb : max_p;
        }
        delta_priv = max_p - min_p;
        delta_ws   = delta_priv; /* rss==private fallback on short ci */
    }

    const double slope = slope_reg_ws;
    const double delta = delta_ws;

    const double day_rate =
        day_total > 0 ? static_cast<double>(day_ok) / static_cast<double>(day_total) : 1.0;
    const double night_rate =
        night_total > 0 ? static_cast<double>(night_ok) / static_cast<double>(night_total) : 1.0;
    const uint64_t disk = dir_size_bytes(work);

    std::printf("stress,profile=%s,day=%d/%d(%.4f),night=%d/%d(%.4f),"
                "rss_slope_reg=%.3f,rss_delta_win=%.3f,"
                "rss_slope_endpoint_ws=%.3f,rss_delta_ws_win=%.3f,"
                "samples=%zu,warmup_skip=%zu,win=%zu,disk_mb=%llu\n",
                profile.name, day_ok, day_total, day_rate, night_ok, night_total, night_rate, slope,
                delta, slope_endpoint_ws, delta_ws, samples.size(), warmup_skip, window_samples,
                static_cast<unsigned long long>(disk / (1024u * 1024u)));

    const char* out_path = std::getenv("BS_DAY19_STRESS_OUT");
    if (out_path && out_path[0])
    {
        std::FILE* f = std::fopen(out_path, "w");
        if (f)
        {
            for (const auto& s : samples)
                std::fprintf(f, "%lld,%.3f,%.3f,%s,%llu,%d\n",
                             static_cast<long long>(s.timestamp_unix), s.rss_mb, s.private_mb,
                             s.phase, static_cast<unsigned long long>(s.iter), s.outcome_ok);
            std::fclose(f);
        }
    }

    const bool diag = (std::getenv("BS_DAY19_RSS_DIAG") != nullptr) ||
                      (std::strcmp(profile.name, "smoke") == 0) ||
                      (std::strcmp(profile.name, "full") == 0);

    bs_test_attach_teardown(&fix);

    if (day_total < profile.min_day_reloads)
    {
        std::fprintf(stderr, "FAIL: day reloads %d < min %d\n", day_total, profile.min_day_reloads);
        return 1;
    }
    if (night_total < profile.min_night_batches)
    {
        std::fprintf(stderr, "FAIL: night batches %d < min %d\n", night_total,
                     profile.min_night_batches);
        return 1;
    }
    if (day_rate < profile.outcome_ok_rate_min || night_rate < profile.outcome_ok_rate_min)
    {
        std::fprintf(stderr, "FAIL: outcome rate below min %.4f\n", profile.outcome_ok_rate_min);
        return 1;
    }
    const bool check_slope = (std::strcmp(profile.name, "ci") != 0);
    if (check_slope && slope > profile.rss_slope_mb_per_hour_max)
    {
        std::fprintf(stderr,
                     "FAIL: rss slope (regression rss) %.3f > max %.3f "
                     "[endpoint_ws=%.3f delta_ws_win=%.3f]\n",
                     slope, profile.rss_slope_mb_per_hour_max, slope_endpoint_ws, delta_ws);
        if (!diag)
            bs_day19_rss_print_samples(samples, stderr);
        return 1;
    }
    if (delta > profile.rss_delta_mb_max)
    {
        std::fprintf(stderr,
                     "FAIL: rss delta (private window) %.3f > max %.3f [delta_ws_win=%.3f]\n",
                     delta, profile.rss_delta_mb_max, delta_ws);
        if (!diag)
            bs_day19_rss_print_samples(samples, stderr);
        return 1;
    }
    if (disk > static_cast<uint64_t>(profile.disk_budget_mb) * 1024u * 1024u)
    {
        std::fprintf(stderr, "FAIL: disk %llu > budget %d MB\n",
                     static_cast<unsigned long long>(disk / (1024u * 1024u)),
                     profile.disk_budget_mb);
        return 1;
    }

    if (diag)
        bs_day19_rss_print_samples(samples, stdout);

    std::printf("Day19StressReloadLoopTest OK\n");
    return 0;
}
