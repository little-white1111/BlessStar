#pragma once

#include <cstddef>
#include <cstdint>
#include <stddef.h>

// C linkage — expects to be invoked from C or plain callback context.
// <stddef.h> guarantees ::size_t in global scope for C linkage functions.
extern "C" void on_config_change_cb(const char* path, int event_type,
                                    const void* data, size_t data_size,
                                    void* user_data);

namespace bs {
namespace app {
namespace sdk {

// Register as ConfigManager WatchCallback adapter.
// Receives config change: memcpy -> ConfigConsumerManager::notify_all
void watch_callback_adapter_init();

} // namespace sdk
} // namespace app
} // namespace bs
