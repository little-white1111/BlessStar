#include "bs/app/sdk/shm/shm_manager.h"
#include <cstring>
#include <cassert>

int main() {
    auto mgr1 = bs::app::sdk::shm::shm_manager::create("test_shm");
    if (!mgr1 || !mgr1->is_valid()) return 1;
    auto mgr2 = bs::app::sdk::shm::shm_manager::attach("test_shm");
    if (!mgr2 || !mgr2->is_valid()) return 1;

    auto* layout = mgr2->get_layout();
    assert(layout != nullptr);
    assert(layout->version_counter == 0);
    assert(layout->active_buf_idx == 0);
    assert(layout->data_len == 0);

    mgr1.reset();
    return 0;
}
