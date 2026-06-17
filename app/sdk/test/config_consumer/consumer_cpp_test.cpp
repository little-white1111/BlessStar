#include "bs/app/sdk/config_consumer.hpp"
#include "bs/app/sdk/shm/double_buffer_writer.h"
#include <cstring>
#include <thread>
#include <cassert>
#include <chrono>

int main() {
    // 1. 先创建 SHM
    auto shm = bs::app::sdk::shm::shm_manager::create("test_cpp");
    assert(shm && shm->is_valid());

    // 2. 构造 ConfigConsumer
    bs::sdk::ConfigConsumer consumer("test_cpp", -1);

    // 3. subscribe
    consumer.subscribe([](const void* data, size_t len, uint64_t ver) {
        (void)data;
        (void)len;
        (void)ver;
    });

    // 4. 用 writer 写入数据
    bs::app::sdk::shm::double_buffer_writer writer(shm.get());
    const char* msg = "Hello CPP Consumer";
    assert(writer.write(msg, strlen(msg) + 1));

    return 0;
}
