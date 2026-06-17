#include "bs/app/sdk/shm/double_buffer_writer.h"
#include "bs/app/sdk/shm/shm_manager.h"
#include <cstring>
#include <cassert>

int main() {
    auto shm = bs::app::sdk::shm::shm_manager::create("test_double_buffer");
    if (!shm || !shm->is_valid()) return 1;

    bs::app::sdk::shm::double_buffer_writer writer(shm.get());

    const char* data1 = "Hello World";
    assert(writer.write(data1, strlen(data1) + 1));
    assert(writer.get_version() == 1);

    const char* data2 = "Testing 123";
    assert(writer.write(data2, strlen(data2) + 1));
    assert(writer.get_version() == 2);

    return 0;
}
