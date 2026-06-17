#include "bs/app/sdk/config_consumer.hpp"
#include "bs/app/sdk/shm/shm_manager.h"
#include "bs/app/sdk/shm/eventfd_notifier.h"
#include "bs/app/sdk/config_consumer.h"

#include <cstring>
#include <condition_variable>
#include <chrono>
#include <mutex>

namespace bs {
namespace sdk {

struct ConfigConsumer::Impl {
    bs::app::sdk::shm::shm_manager* shm_manager_ = nullptr;
    bs::app::sdk::shm::eventfd_notifier* notifier_ = nullptr;
    uint64_t cached_version_ = 0;
    std::vector<uint8_t> buffer_;
    size_t data_len_ = 0;

    OnChangeCallback callback_;

    std::mutex mutex_;
    std::condition_variable cv_;
    bool changed_ = false;

    Impl(bs::app::sdk::shm::shm_manager* shm,
         bs::app::sdk::shm::eventfd_notifier* notifier)
        : shm_manager_(shm), notifier_(notifier) {}

    ~Impl() {
        if (shm_manager_) {
            shm_manager_->detach();
            delete shm_manager_;
            shm_manager_ = nullptr;
        }
        if (notifier_) {
            notifier_->close();
            delete notifier_;
            notifier_ = nullptr;
        }
    }
};

ConfigConsumer::ConfigConsumer(const std::string& shm_path, int efd)
    : impl_(nullptr) {
    (void)efd;
    std::string key = shm_path;
    auto pos = key.rfind('/');
    if (pos != std::string::npos) key = key.substr(pos + 1);

    auto* shm = bs::app::sdk::shm::shm_manager::attach(key).release();
    bs::app::sdk::shm::eventfd_notifier* notifier = nullptr;
    try {
        notifier = new bs::app::sdk::shm::eventfd_notifier();
    } catch (...) {
        notifier = nullptr;
    }
    impl_ = std::make_unique<Impl>(shm, notifier);
}

ConfigConsumer::~ConfigConsumer() {
    impl_.reset();
}

ConfigConsumer::ConfigConsumer(ConfigConsumer&& other) noexcept
    : impl_(std::move(other.impl_)) {}

ConfigConsumer& ConfigConsumer::operator=(ConfigConsumer&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void ConfigConsumer::subscribe(OnChangeCallback callback) {
    if (!impl_) return;
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->callback_ = callback;
}

bool ConfigConsumer::wait_and_read(int timeout_ms) {
    if (!impl_ || !impl_->shm_manager_ || !impl_->shm_manager_->is_valid())
        return false;

    {
        std::unique_lock<std::mutex> lock(impl_->mutex_);
        if (timeout_ms < 0) {
            impl_->cv_.wait(lock, [this]() { return impl_->changed_; });
        } else {
            auto timeout = std::chrono::milliseconds(timeout_ms);
            if (!impl_->cv_.wait_for(lock, timeout,
                                     [this]() { return impl_->changed_; })) {
                return false;
            }
        }
        impl_->changed_ = false;
    }

    auto* layout = impl_->shm_manager_->get_layout();
    uint64_t version = layout->version_counter;
    if (version != impl_->cached_version_) {
        size_t len = layout->data_len;
        if (len > 0) {
            impl_->buffer_.resize(len);
            uint64_t active = layout->active_buf_idx;
            const uint8_t* src = (active == 0) ? layout->bufA : layout->bufB;
            std::memcpy(impl_->buffer_.data(), src, len);
            impl_->cached_version_ = version;
            impl_->data_len_ = len;
        }
    }

    if (impl_->callback_ && impl_->data_len_ > 0) {
        impl_->callback_(impl_->buffer_.data(), impl_->data_len_,
                         impl_->cached_version_);
    }

    return true;
}

const void* ConfigConsumer::data(size_t& out_len) const {
    if (!impl_ || impl_->data_len_ == 0) {
        out_len = 0;
        return nullptr;
    }
    out_len = impl_->data_len_;
    return impl_->buffer_.data();
}

uint64_t ConfigConsumer::version() const {
    if (!impl_ || !impl_->shm_manager_ || !impl_->shm_manager_->is_valid())
        return 0;
    return impl_->shm_manager_->get_layout()->version_counter;
}

} // namespace sdk
} // namespace bs
