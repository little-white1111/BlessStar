#pragma once

#include "bs/app/sdk/shm/shm_manager.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace bs {
namespace app {
namespace sdk {
namespace shm {

class double_buffer_writer {
public:
    explicit double_buffer_writer(shm_manager* manager);
    ~double_buffer_writer() = default;

    double_buffer_writer(const double_buffer_writer&) = delete;
    double_buffer_writer& operator=(const double_buffer_writer&) = delete;
    double_buffer_writer(double_buffer_writer&&) = default;
    double_buffer_writer& operator=(double_buffer_writer&&) = default;

    bool write(const void* data, size_t len);

    uint64_t get_version() const noexcept;

private:
    shm_manager* manager_;
    std::atomic<uint64_t> local_version_{0};
};

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs
