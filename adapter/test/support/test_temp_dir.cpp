#include <chrono>
#include <cstdint>

#include <atomic>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "test_temp_dir.h"

fs::path bs_test_unique_temp_dir(const char* prefix)
{
    static std::atomic<uint32_t> counter{0};
    // clang-format off
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#ifdef _WIN32
    const uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
    const uint32_t pid = static_cast<uint32_t>(getpid());
#endif
    const uint32_t salt = counter.fetch_add(1, std::memory_order_relaxed);
    // clang-format on
    std::ostringstream dir_name;
    dir_name << prefix << '_' << pid << '_' << now_ns << '_' << salt;
    const fs::path  tmp = fs::temp_directory_path() / dir_name.str();
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp);
    return tmp;
}

std::string bs_test_path_to_file_uri(const fs::path& path)
{
    std::string uri = "file:///" + fs::absolute(path).string();
    for (char& c : uri)
    {
        if (c == '\\')
            c = '/';
    }
    return uri;
}

bool bs_test_write_binary_file(const fs::path& path, const void* data, std::size_t len)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
    return out.good();
}

BsTestTempDirGuard::BsTestTempDirGuard(fs::path p)
    : path(std::move(p))
{
}

BsTestTempDirGuard::~BsTestTempDirGuard()
{
    std::error_code ec;
    fs::remove_all(path, ec);
}
