#include "bs/app/sdk/shm/eventfd_notifier.h"

#include <cstdint>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#endif

namespace bs {
namespace app {
namespace sdk {
namespace shm {

eventfd_notifier::eventfd_notifier() {
#ifdef _WIN32
    // Windows: eventfd not available, stub with -1
    efd_ = -1;
#else
    efd_ = eventfd(0, EFD_SEMAPHORE | EFD_CLOEXEC);
    if (efd_ == -1) {
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
#endif
}

eventfd_notifier::~eventfd_notifier() {
    close();
}

eventfd_notifier::eventfd_notifier(eventfd_notifier&& other) noexcept
    : efd_(other.efd_) {
    other.efd_ = -1;
}

eventfd_notifier& eventfd_notifier::operator=(eventfd_notifier&& other) noexcept {
    if (this != &other) {
        close();
        efd_ = other.efd_;
        other.efd_ = -1;
    }
    return *this;
}

bool eventfd_notifier::notify() {
    if (efd_ == -1) return false;
#ifdef _WIN32
    // TODO: Windows eventfd stub — implement via pipe or I/O completion port
    (void)efd_;
    return false;
#else
    uint64_t val = 1;
    ssize_t ret = write(efd_, &val, sizeof(val));
    return ret == static_cast<ssize_t>(sizeof(val));
#endif
}

void eventfd_notifier::close() {
    if (efd_ != -1) {
#ifdef _WIN32
        // no-op for stub
#else
        ::close(efd_);
#endif
        efd_ = -1;
    }
}

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs
