#include "bs/app/sdk/shm/double_buffer_writer.h"
#include <cstring>
#include <atomic>

namespace bs {
namespace app {
namespace sdk {
namespace shm {

double_buffer_writer::double_buffer_writer(shm_manager* manager)
    : manager_(manager) {}

bool double_buffer_writer::write(const void* data, size_t len) {
    if (!manager_ || !manager_->is_valid()) return false;
    auto* layout = manager_->get_layout();
    if (len > SHM_BUF_SIZE) return false;

    uint64_t active_idx = layout->active_buf_idx;
    uint64_t target = active_idx ^ 1;

    uint8_t* buf_ptr = (target == 0) ? layout->bufA : layout->bufB;
    std::memcpy(buf_ptr, data, len);

    std::atomic_thread_fence(std::memory_order_release);
    layout->data_len = len;
    std::atomic_thread_fence(std::memory_order_release);
    layout->version_counter++;
    std::atomic_thread_fence(std::memory_order_release);
    layout->active_buf_idx = target;

    return true;
}

uint64_t double_buffer_writer::get_version() const noexcept {
    if (!manager_ || !manager_->is_valid()) return 0;
    return manager_->get_layout()->version_counter;
}

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs
