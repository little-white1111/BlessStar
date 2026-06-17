#include "bs/app/sdk/config_consumer.h"
#include "bs/app/sdk/shm/shm_manager.h"
#include "bs/app/sdk/shm/eventfd_notifier.h"
#include "bs/app/sdk/config_consumer_manager.h"
#include <cstring>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <string>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <cerrno>
#include <system_error>

struct bs_consumer_t {
    bs::app::sdk::shm::shm_manager* shm_manager_;
    bs::app::sdk::shm::eventfd_notifier* notifier_;
    uint64_t cached_version_ = 0;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool changed_ = false;
    bs_consumer_on_change_fn on_change_fn_ = nullptr;
    void* user_data_ = nullptr;
    uint8_t buffer_[64 * 1024];
    size_t data_len_ = 0;

    bs_consumer_t() : shm_manager_(nullptr), notifier_(nullptr) {}

    ~bs_consumer_t() {
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

// Internal hook used by watch_callback_adapter
void trigger_consumer_notify(bs_consumer_t* consumer, const void* data, size_t len) {
    if (!consumer || !data) return;
    std::lock_guard<std::mutex> lock(consumer->mutex_);
    if (len > sizeof(consumer->buffer_)) len = sizeof(consumer->buffer_);
    std::memcpy(consumer->buffer_, data, len);
    consumer->data_len_ = len;
    consumer->changed_ = true;
    consumer->cv_.notify_all();
    if (consumer->on_change_fn_) {
        consumer->on_change_fn_(consumer->buffer_, consumer->data_len_,
                                consumer->cached_version_, consumer->user_data_);
    }
}

bs_consumer_t* bs_consumer_create(const char* shm_path, int efd) {
    if (!shm_path) return nullptr;

    std::string path(shm_path);
    auto pos = path.rfind('/');
    std::string config_key = (pos != std::string::npos) ? path.substr(pos + 1) : path;

    auto consumer = new bs_consumer_t();
    consumer->shm_manager_ = bs::app::sdk::shm::shm_manager::attach(config_key).release();
    if (!consumer->shm_manager_ || !consumer->shm_manager_->is_valid()) {
        delete consumer;
        return nullptr;
    }

    try {
        consumer->notifier_ = new bs::app::sdk::shm::eventfd_notifier();
    } catch (...) {
        consumer->notifier_ = nullptr;
    }

    return consumer;
}

int bs_consumer_wait(bs_consumer_t* consumer, int timeout_ms) {
    if (!consumer) return -1;
    std::unique_lock<std::mutex> lock(consumer->mutex_);
    if (timeout_ms < 0) {
        consumer->cv_.wait(lock, [consumer]() { return consumer->changed_; });
    } else {
        auto timeout = std::chrono::milliseconds(timeout_ms);
        if (!consumer->cv_.wait_for(lock, timeout, [consumer]() {
                return consumer->changed_;
            })) {
            return -2;
        }
    }
    consumer->changed_ = false;
    return 0;
}

const void* bs_consumer_get_data(bs_consumer_t* consumer, size_t* out_len) {
    if (!consumer || !consumer->shm_manager_ || !consumer->shm_manager_->is_valid()) {
        if (out_len) *out_len = 0;
        return nullptr;
    }
    auto layout = consumer->shm_manager_->get_layout();
    uint64_t version = layout->version_counter;
    if (version != consumer->cached_version_) {
        size_t len = layout->data_len;
        if (len > 0 && len <= sizeof(consumer->buffer_)) {
            uint64_t active = layout->active_buf_idx;
            const uint8_t* src = (active == 0) ? layout->bufA : layout->bufB;
            std::memcpy(consumer->buffer_, src, len);
            consumer->cached_version_ = version;
            consumer->data_len_ = len;
        }
    }
    if (out_len) *out_len = consumer->data_len_;
    return (consumer->data_len_ > 0) ? consumer->buffer_ : nullptr;
}

uint64_t bs_consumer_get_version(bs_consumer_t* consumer) {
    if (!consumer || !consumer->shm_manager_ || !consumer->shm_manager_->is_valid()) return 0;
    return consumer->shm_manager_->get_layout()->version_counter;
}

void bs_consumer_set_on_change(bs_consumer_t* consumer, bs_consumer_on_change_fn fn, void* userdata) {
    if (!consumer) return;
    std::lock_guard<std::mutex> lock(consumer->mutex_);
    consumer->on_change_fn_ = fn;
    consumer->user_data_ = userdata;
}

void bs_consumer_destroy(bs_consumer_t* consumer) {
    delete consumer;
}
