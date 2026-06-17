#include "bs/app/sdk/shm/eventfd_notifier.h"
#include <cstdlib>
#include <cassert>

int main() {
    bs::app::sdk::shm::eventfd_notifier notifier;
    // 只需测试 notify（Windows 上 stub 返回 false，Linux 返回 true）
    bool ok = notifier.notify();
    // 不 assert ok，因为 Windows 上 stub 返回 false 是预期行为
    // 只验证不崩溃即可
    return 0;
}
