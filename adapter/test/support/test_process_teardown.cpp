/**
 * Process-exit log teardown for CI LeakSanitizer (runs after main via destructor).
 * Linked only into tests that pull in bs_adapter_log / bs_adapter_registry.
 */

#include "bs/adapter/registry_bootstrap.h"

#if defined(__GNUC__) || defined(__clang__)
__attribute__((destructor))
#endif
static void bs_test_process_log_teardown(void)
{
    bs_adapter_registry_shutdown_log();
}
