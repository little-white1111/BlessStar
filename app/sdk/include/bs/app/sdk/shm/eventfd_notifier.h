#pragma once

#include <cstdint>
#include <system_error>

namespace bs {
namespace app {
namespace sdk {
namespace shm {

class eventfd_notifier {
public:
    eventfd_notifier();
    ~eventfd_notifier();

    eventfd_notifier(const eventfd_notifier&) = delete;
    eventfd_notifier& operator=(const eventfd_notifier&) = delete;
    eventfd_notifier(eventfd_notifier&& other) noexcept;
    eventfd_notifier& operator=(eventfd_notifier&& other) noexcept;

    bool notify();
    int native_fd() const noexcept { return efd_; }
    void close();

private:
    int efd_ = -1;
};

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs
