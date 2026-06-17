#include "bs/app/sdk/config_consumer.hpp"
#include "bs/app/sdk/config_consumer.h"
#include "bs/app/sdk/shm/double_buffer_writer.h"
#include "bs/app/sdk/config_consumer_manager.h"
#include <cstring>
#include <thread>
#include <chrono>
#include <cassert>
#include <atomic>

int main() {
    auto shm = bs::app::sdk::shm::shm_manager::create("test_e2e");
    assert(shm && shm->is_valid());

    bs::app::sdk::shm::double_buffer_writer writer(shm.get());
    bs::sdk::ConfigConsumer consumer("test_e2e", -1);

    std::atomic<bool> cb_fired(false);
    consumer.subscribe([&](const void* data, size_t len, uint64_t ver) {
        cb_fired = true;
        (void)data; (void)len; (void)ver;
    });

    const char* payload = "E2E Test Payload";
    assert(writer.write(payload, strlen(payload) + 1));
    assert(writer.get_version() == 1);

    size_t out_len = 0;
    const void* got = consumer.data(out_len);
    assert(got != nullptr);
    assert(out_len == strlen(payload) + 1);
    assert(std::memcmp(got, payload, out_len) == 0);

    uint64_t ver = consumer.version();
    assert(ver == 1);

    return 0;
}
