/**
 * Hermetic temp directories for adapter integration tests (C-ST-10).
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

/** Unique dir under system temp; removes any prior path with the same name. */
fs::path bs_test_unique_temp_dir(const char* prefix);

std::string bs_test_path_to_file_uri(const fs::path& path);

bool bs_test_write_binary_file(const fs::path& path, const void* data, std::size_t len);

struct BsTestTempDirGuard
{
    fs::path path;

    explicit BsTestTempDirGuard(fs::path p);
    BsTestTempDirGuard(const BsTestTempDirGuard&)            = delete;
    BsTestTempDirGuard& operator=(const BsTestTempDirGuard&) = delete;
    ~BsTestTempDirGuard();
};
