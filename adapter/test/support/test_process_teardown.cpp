/**
 * Process-exit log teardown for CI LeakSanitizer (sanitizer build only).
 */

#include "bs/adapter/registry_bootstrap.h"

#if defined(BLESSSTAR_SANITIZER_CI)

#if defined(__GNUC__) || defined(__clang__)
__attribute__((destructor)) static void bs_test_process_log_teardown(void);
#else
static void bs_test_process_log_teardown(void);
#endif

static void bs_test_process_log_teardown(void)
{
    bs_adapter_registry_shutdown_log();
}

#endif /* BLESSSTAR_SANITIZER_CI */
