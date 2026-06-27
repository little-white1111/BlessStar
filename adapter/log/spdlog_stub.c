/* ── spdlog_stub.c ─────────────────────────────────────────────────────
 * Stub implementations of spdlog-based log bus functions.
 * Compiled only when real spdlog is not available.
 * These are NOT behind BLESSSTAR_SANITIZER_CI guard because they're
 * needed by registry_bootstrap.cpp in all build configurations.
 */

#include "bs/adapter/attach_context.h"
#include "bs/adapter/log/log_bus.h"

int bs_adapter_log_bind_spdlog_bus(void)
{
    /* No spdlog available — return error */
    return -1;
}

void bs_adapter_log_shutdown_if_bound(void)
{
    bs_adapter_attach_ctx_shutdown_all_logs();
    bs_adapter_attach_mark_log_ready(0);
}
