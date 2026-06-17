#include "bs/app/sdk/watch_callback_adapter.h"
#include "bs/app/sdk/config_consumer_manager.h"
#include <cstring>
#include <cstdlib>
#include <stddef.h>

// Must be at file scope (outside any namespace) for C linkage correctness.
// Header already declares it as extern "C", so this definition matches.
extern "C" void on_config_change_cb(const char* path, int event_type,
                                    const void* data, size_t data_size,
                                    void* user_data) {
    (void)event_type;
    (void)user_data;

    if (!path || !data || data_size == 0) return;

    void* copy = std::malloc(data_size);
    if (!copy) return;
    std::memcpy(copy, data, data_size);

    bs::app::sdk::ConfigConsumerManager::Instance().notify_all(path, copy, data_size);

    std::free(copy);
}

namespace bs {
namespace app {
namespace sdk {

void watch_callback_adapter_init() {
}

} // namespace sdk
} // namespace app
} // namespace bs
