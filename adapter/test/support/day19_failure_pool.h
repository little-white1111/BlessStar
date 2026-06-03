/**
 * Mixed good/bad path pool for Day19 failure-stress profile (smoke_fail).
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <filesystem>

#include "day19_fixture_gen.h"
#include "test_temp_dir.h"

namespace fs = std::filesystem;

enum BsDay19PathKind : int
{
    BS_DAY19_PATH_GOOD       = 0,
    BS_DAY19_PATH_PARSE_FAIL = 1,
    BS_DAY19_PATH_GATE_FAIL  = 2,
    BS_DAY19_PATH_READ_FAIL  = 3
};

struct BsDay19PathEntry
{
    std::string uri;
    BsDay19PathKind kind = BS_DAY19_PATH_GOOD;
};

inline std::string bs_day19_gate_reject_v1_json()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"__day19_gate_reject__","name":"x"}]})";
}

/** good_count files + one parse/gate/read fault URI each; day cycle uses full list. */
inline int bs_day19_write_failure_path_pool(const fs::path& work, int good_count, size_t fixture_bytes,
                                            std::vector<BsDay19PathEntry>* out)
{
    if (!out || good_count < 1)
        return -1;
    out->clear();

    const std::string good_json = bs_day19_make_valid_v1_json(fixture_bytes);
    for (int i = 0; i < good_count; ++i)
    {
        const fs::path f = work / ("good-" + std::to_string(i) + ".json");
        if (!bs_test_write_binary_file(f, good_json.data(), good_json.size()))
            return -1;
        out->push_back({bs_test_path_to_file_uri(f), BS_DAY19_PATH_GOOD});
    }

    {
        const fs::path f = work / "bad-parse.json";
        const char*    bad = "{not-valid-json";
        if (!bs_test_write_binary_file(f, bad, std::strlen(bad)))
            return -1;
        out->push_back({bs_test_path_to_file_uri(f), BS_DAY19_PATH_PARSE_FAIL});
    }

    {
        const fs::path       f = work / "bad-gate.json";
        const std::string    gj = bs_day19_gate_reject_v1_json();
        if (!bs_test_write_binary_file(f, gj.data(), gj.size()))
            return -1;
        out->push_back({bs_test_path_to_file_uri(f), BS_DAY19_PATH_GATE_FAIL});
    }

    {
        const fs::path f = work / "bad-read-missing.json";
        out->push_back({bs_test_path_to_file_uri(f), BS_DAY19_PATH_READ_FAIL});
    }

    return 0;
}
