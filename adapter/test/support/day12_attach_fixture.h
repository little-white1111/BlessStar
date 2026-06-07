#pragma once

#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/persistence/attach_store.h"

struct AttachContext;

/** Wire RES-IX session defaults for unit/integration tests (in-memory manifest). */
inline void day12_wire_reload_defaults(ReloadBatchController* ctrl,
                                       BsAttachScheme         scheme = BS_ATTACH_SCHEME_PER_PATH,
                                       AttachContext*         ctx    = nullptr)
{
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, scheme);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, nullptr);
    if (ctx)
        bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, ctx);
}
