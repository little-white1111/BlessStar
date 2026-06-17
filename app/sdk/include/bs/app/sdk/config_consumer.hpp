#pragma once

#include <functional>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bs {
namespace sdk {

class ConfigConsumer {
public:
    ConfigConsumer(const std::string& shm_path, int efd);
    ~ConfigConsumer();

    ConfigConsumer(const ConfigConsumer&) = delete;
    ConfigConsumer& operator=(const ConfigConsumer&) = delete;
    ConfigConsumer(ConfigConsumer&& other) noexcept;
    ConfigConsumer& operator=(ConfigConsumer&& other) noexcept;

    using OnChangeCallback = std::function<void(const void* data, size_t len, uint64_t version)>;
    void subscribe(OnChangeCallback callback);
    bool wait_and_read(int timeout_ms = -1);
    const void* data(size_t& out_len) const;
    uint64_t version() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sdk
} // namespace bs
