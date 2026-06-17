#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#endif

namespace bs {
namespace app {
namespace sdk {
namespace shm {

struct shm_config_layout {
    uint64_t version_counter;    // +0x00
    uint64_t active_buf_idx;     // +0x08
    uint64_t data_len;           // +0x10
    uint8_t  reserved[40];       // +0x18
    // +0x40
    uint8_t  bufA[64 * 1024];    // 64KB buffer
    uint8_t  bufB[64 * 1024];    // 64KB buffer
};

static constexpr size_t SHM_BUF_SIZE = 64 * 1024;
static constexpr size_t SHM_HEAD_SIZE = offsetof(shm_config_layout, bufA);
static constexpr size_t SHM_TOTAL_SIZE = sizeof(shm_config_layout);

class shm_manager {
public:
    shm_manager() = default;
    ~shm_manager();

    shm_manager(const shm_manager&) = delete;
    shm_manager& operator=(const shm_manager&) = delete;

    shm_manager(shm_manager&& other) noexcept;
    shm_manager& operator=(shm_manager&& other) noexcept;

    static std::unique_ptr<shm_manager> create(const std::string& config_key);
    static std::unique_ptr<shm_manager> attach(const std::string& config_key);

    void detach();

    shm_config_layout* get_layout() noexcept { return layout_; }
    const shm_config_layout* get_layout() const noexcept { return layout_; }
    const std::string& get_name() const noexcept { return name_; }

    bool is_valid() const noexcept { return layout_ != nullptr; }

private:
    shm_manager(const std::string& config_key);

    std::string name_;
    void* backing_fd_ = nullptr;
    shm_config_layout* layout_ = nullptr;
};

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs