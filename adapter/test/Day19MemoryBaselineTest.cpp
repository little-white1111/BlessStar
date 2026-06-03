/**
 * T19.1 / T19.2: cold vs hot load RSS baseline (1KB / 100KB; optional 10MB via env).
 */

#include "bs/kernel/common/bs_status.h"

#include "bs/adapter/parser/config_parse.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "support/day19_fixture_gen.h"
#include "support/day19_rss_sampler.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int parse_file_rss_mb(const fs::path& path, double* rss_out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return -1;
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    BsConfigParseResult result{};
    const BsStatus st = bs_adapter_parser_parse_bytes(
        reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &result);
    if (!bs_status_is_ok(st))
        return -1;
    bs_adapter_parser_result_destroy(&result);
    *rss_out = static_cast<double>(bs_day19_current_rss_bytes()) / (1024.0 * 1024.0);
    return 0;
}

static int run_size_label(const fs::path& work, const char* label, size_t target_bytes,
                          std::vector<BsDay19RssSample>& samples)
{
    const fs::path cfg = work / (std::string("cfg_") + label + ".json");
    const std::string json = bs_day19_make_valid_v1_json(target_bytes);
    if (!bs_test_write_binary_file(cfg, json.data(), json.size()))
        return -1;

    double rss = 0.0;
    if (parse_file_rss_mb(cfg, &rss) != 0)
        return -1;
    bs_day19_rss_sample_push(samples, "cold", 0, 1);
    samples.back().rss_mb = rss;

    if (parse_file_rss_mb(cfg, &rss) != 0)
        return -1;
    bs_day19_rss_sample_push(samples, "hot", 1, 1);
    samples.back().rss_mb = rss;

    std::printf("baseline,%s,cold_mb=%.3f,hot_mb=%.3f\n", label, samples[samples.size() - 2].rss_mb,
                samples.back().rss_mb);
    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    const BsTestTempDirGuard tmp(bs_test_unique_temp_dir("bs_day19_mem_baseline"));
    std::vector<BsDay19RssSample> samples;

    if (run_size_label(tmp.path, "1kb", 1024u, samples) != 0)
    {
        std::fprintf(stderr, "FAIL: 1kb baseline\n");
        return 1;
    }
    if (run_size_label(tmp.path, "100kb", 100u * 1024u, samples) != 0)
    {
        std::fprintf(stderr, "FAIL: 100kb baseline\n");
        return 1;
    }

    const char* run_10mb = std::getenv("BS_DAY19_BASELINE_10MB");
    if (run_10mb && run_10mb[0] == '1')
    {
        if (run_size_label(tmp.path, "10mb", 10u * 1024u * 1024u, samples) != 0)
        {
            std::fprintf(stderr, "FAIL: 10mb baseline\n");
            return 1;
        }
    }
    else
    {
        std::printf("baseline,10mb,skipped (set BS_DAY19_BASELINE_10MB=1 to enable)\n");
    }

    const char* out_path = std::getenv("BS_DAY19_BASELINE_OUT");
    if (out_path && out_path[0])
    {
        std::ofstream out(out_path, std::ios::trunc);
        if (out)
        {
            out << "timestamp,rss_mb,phase,iter,outcome_ok\n";
            for (const auto& s : samples)
                out << s.timestamp_unix << ',' << s.rss_mb << ',' << s.phase << ',' << s.iter << ','
                    << s.outcome_ok << '\n';
        }
    }

    std::printf("Day19MemoryBaselineTest OK (%zu samples)\n", samples.size());
    return 0;
}
