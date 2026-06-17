#include "bs/app/sdk/config_consumer_manager.h"
#include "bs/app/sdk/shm/shm_manager.h"
#include "bs/app/sdk/shm/double_buffer_writer.h"
#include "bs/app/sdk/shm/eventfd_notifier.h"

#include <cstring>
#include <memory>

namespace bs {
namespace app {
namespace sdk {

ConfigConsumerManager& ConfigConsumerManager::Instance() {
    static ConfigConsumerManager instance;
    return instance;
}

uint64_t ConfigConsumerManager::register_consumer(const std::string& config_key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto ctx = std::make_unique<ConsumerContext>();
    ctx->config_key = config_key;

    auto mgr = shm::shm_manager::create(config_key);
    ctx->shm_ptr = mgr->get_layout();

    uint64_t id = next_id_++;
    consumers_[id] = std::move(ctx);
    return id;
}

void ConfigConsumerManager::unregister_consumer(uint64_t consumer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    consumers_.erase(consumer_id);
}

void ConfigConsumerManager::notify_all(const std::string& config_key,
                                       const void* data, size_t len) {
    (void)config_key;
    (void)data;
    (void)len;
}

} // namespace sdk
} // namespace app
} // namespace bs
