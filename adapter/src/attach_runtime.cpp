#include "bs/adapter/attach_runtime.h"

#include "bs/adapter/attach_context.h"

int bs_adapter_attach_is_log_ready(void)
{
    AttachContext* ctx = bs_attach_context_get_active();
    if (!ctx)
        return 0;
    return bs_attach_context_is_log_bus_bound(ctx) ? 1 : 0;
}

void bs_adapter_attach_mark_log_ready(int ready)
{
    AttachContext* ctx = bs_attach_context_get_active();
    if (ctx)
        bs_attach_context_set_log_bus_bound(ctx, ready ? 1 : 0);
}
