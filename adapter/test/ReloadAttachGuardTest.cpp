/** LOG-VII-10: bs_adapter_attach_reload_batch_run rejects when log bus is not bound. */

#include "bs/adapter/attach_context.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cassert>

static int noop_read(void*, const char*, IoReadResult* out)
{
    bs_io_read_result_init(out);
    out->status = BS_IO_OK;
    return BS_IO_OK;
}

int main()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);
    bs_adapter_attach_ctx_set_active(ctx);
    bs_adapter_attach_ctx_set_log_bus_bound(ctx, 0);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    assert(ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, ctx);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, noop_read, nullptr);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    assert(bs_adapter_attach_reload_batch_add_path(ctrl, "file:///x") == 0);
    assert(bs_adapter_attach_reload_batch_run(ctrl) == -2);

    bs_adapter_attach_reload_batch_destroy(ctrl);
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}
