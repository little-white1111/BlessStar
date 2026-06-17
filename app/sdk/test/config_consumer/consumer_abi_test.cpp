#include "bs/app/sdk/config_consumer.h"
#include "bs/app/sdk/shm/shm_manager.h"
#include <cstring>
#include <cassert>

int main() {
    auto mgr = bs::app::sdk::shm::shm_manager::create("test_abi");
    assert(mgr && mgr->is_valid());

    bs_consumer_t* c = bs_consumer_create("test_abi", -1);
    assert(c != nullptr);

    uint64_t v = bs_consumer_get_version(c);
    assert(v == 0);

    bs_consumer_set_on_change(c, nullptr, nullptr);
    bs_consumer_destroy(c);

    mgr.reset();
    return 0;
}
