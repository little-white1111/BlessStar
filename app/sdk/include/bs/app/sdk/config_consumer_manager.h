#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace bs {
namespace app {
namespace sdk {

struct ConsumerContext {
    std::string config_key;
    void* shm_ptr = nullptr;
    int eventfd_fd = -1;
};

class ConfigConsumerManager {
public:
    static ConfigConsumerManager& Instance();

    uint64_t register_consumer(const std::string& config_key);
    void unregister_consumer(uint64_t consumer_id);
    void notify_all(const std::string& config_key, const void* data, size_t len);

private:
    ConfigConsumerManager() = default;
    ~ConfigConsumerManager() = default;
    ConfigConsumerManager(const ConfigConsumerManager&) = delete;

    std::mutex mutex_;
    std::unordered_map<uint64_t, std::unique_ptr<ConsumerContext>> consumers_;
    uint64_t next_id_ = 1;
};

} // namespace sdk
} // namespace app
} // namespace bs
