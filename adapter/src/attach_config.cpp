#include "bs/kernel/state/ConfigManager.h"
#include "bs/kernel/state/ConfigState.h"

#include "bs/adapter/attach_config.h"

#include <cstdlib>
#include <cstring>

#include "attach_context_internal.h"

static const char* g_sync_fail_path = nullptr;

int bs_adapter_attach_ctx_init_config_manager(AttachContext* ctx)
{
    if (!ctx || ctx->config_manager)
        return -1;
    ctx->config_manager = bs_config_manager_create();
    return ctx->config_manager ? 0 : -1;
}

void bs_adapter_attach_ctx_destroy_config_manager(AttachContext* ctx)
{
    if (!ctx || !ctx->config_manager)
        return;
    bs_config_manager_destroy(ctx->config_manager);
    ctx->config_manager = nullptr;
}

int bs_adapter_attach_config_get_state(AttachContext* ctx, const char* config_path,
                                       ConfigState* state)
{
    ConfigManager* cm = bs_adapter_attach_ctx_config_manager(ctx);
    if (!cm || !config_path || !state)
        return -1;
    return bs_config_manager_get_config_state(cm, config_path, state);
}

int bs_adapter_attach_config_get_snapshot(AttachContext* ctx, const char* config_path, void** data,
                                          size_t* size)
{
    ConfigManager* cm = bs_adapter_attach_ctx_config_manager(ctx);
    if (!cm || !config_path || !data || !size)
        return -1;
    return bs_config_manager_get_config_snapshot(cm, config_path, data, size);
}

int bs_adapter_attach_config_has_manager(AttachContext* ctx)
{
    return (ctx && bs_adapter_attach_ctx_config_manager(ctx)) ? 1 : 0;
}

int bs_adapter_attach_config_sync_path(AttachContext* ctx, const char* config_path,
                                       const void* data, size_t data_size)
{
    ConfigManager* cm = bs_adapter_attach_ctx_config_manager(ctx);
    if (!cm || !config_path)
        return -1;
    if (data_size > 0 && !data)
        return -1;

#if defined(BS_TESTING)
    if (g_sync_fail_path && std::strcmp(config_path, g_sync_fail_path) == 0)
        return -99;
#endif

    ConfigState state = CONFIG_STATE_INITIAL;
    const int   st_rc = bs_config_manager_get_config_state(cm, config_path, &state);

    /* B-01: path not loaded / terminal INITIAL|CLOSED -> load_config. */
    if (st_rc == -2 || state == CONFIG_STATE_CLOSED || state == CONFIG_STATE_INITIAL)
        return bs_config_manager_load_config(cm, config_path, data, data_size);

    if (st_rc != 0)
        return st_rc;

    /* B-02: ACTIVE -> hot_update; LOADING/UPDATING/ERROR -> reload_config. */
    if (state == CONFIG_STATE_ACTIVE)
        return bs_config_manager_hot_update(cm, config_path, data, data_size);

    return bs_config_manager_reload_config(cm, config_path, data, data_size);
}

int bs_adapter_attach_config_checkpoint_path(AttachContext* ctx, const char* config_path,
                                             BsAttachConfigPathCheckpoint* out)
{
    if (!out)
        return -1;
    out->had_prior   = 0;
    out->prior_state = CONFIG_STATE_INITIAL;
    out->prior_data  = nullptr;
    out->prior_size  = 0;

    ConfigManager* cm = bs_adapter_attach_ctx_config_manager(ctx);
    if (!cm || !config_path)
        return -1;

    ConfigState state = CONFIG_STATE_INITIAL;
    const int   st_rc = bs_config_manager_get_config_state(cm, config_path, &state);
    if (st_rc == -2)
        return 0;

    if (st_rc != 0)
        return st_rc;

    out->had_prior   = 1;
    out->prior_state = state;

    void*  snap = nullptr;
    size_t sz   = 0;
    if (bs_config_manager_get_config_snapshot(cm, config_path, &snap, &sz) != 0)
        return -1;

    if (sz > 0 && snap)
    {
        out->prior_data = std::malloc(sz);
        if (!out->prior_data)
        {
            std::free(snap);
            return -1;
        }
        std::memcpy(out->prior_data, snap, sz);
        out->prior_size = sz;
    }

    std::free(snap);
    return 0;
}

void bs_adapter_attach_config_checkpoint_release(BsAttachConfigPathCheckpoint* checkpoint)
{
    if (!checkpoint)
        return;
    if (checkpoint->prior_data)
        std::free(checkpoint->prior_data);
    checkpoint->prior_data  = nullptr;
    checkpoint->prior_size  = 0;
    checkpoint->had_prior   = 0;
    checkpoint->prior_state = CONFIG_STATE_INITIAL;
}

int bs_adapter_attach_config_rollback_path(AttachContext* ctx, const char* config_path,
                                           const BsAttachConfigPathCheckpoint* checkpoint)
{
    ConfigManager* cm = bs_adapter_attach_ctx_config_manager(ctx);
    if (!cm || !config_path)
        return -1;

    if (!checkpoint || !checkpoint->had_prior)
        return bs_config_manager_unload_config(cm, config_path);

    if (checkpoint->prior_state == CONFIG_STATE_CLOSED ||
        checkpoint->prior_state == CONFIG_STATE_INITIAL)
        return bs_config_manager_unload_config(cm, config_path);

    if (checkpoint->prior_state == CONFIG_STATE_ACTIVE)
    {
        if (checkpoint->prior_size > 0 && checkpoint->prior_data)
            return bs_config_manager_hot_update(cm, config_path, checkpoint->prior_data,
                                                checkpoint->prior_size);
        return bs_config_manager_unload_config(cm, config_path);
    }

    if (checkpoint->prior_size > 0 && checkpoint->prior_data)
        return bs_config_manager_reload_config(cm, config_path, checkpoint->prior_data,
                                               checkpoint->prior_size);

    return bs_config_manager_unload_config(cm, config_path);
}

/** C-01: freeze marker enters ConfigManager via sync_path (LOADING->ACTIVE on first load). */
int bs_adapter_attach_notify_registry_frozen(AttachContext* ctx)
{
    static const char k_marker[] = "attach-frozen";
    return bs_adapter_attach_config_sync_path(ctx, BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN, k_marker,
                                              sizeof(k_marker) - 1);
}

EventBus* bs_adapter_attach_config_event_bus(AttachContext* ctx)
{
    ConfigManager* cm = bs_adapter_attach_ctx_config_manager(ctx);
    return cm ? bs_config_manager_get_event_bus(cm) : nullptr;
}

void bs_adapter_attach_config_testing_set_sync_fail_path(const char* config_path)
{
    g_sync_fail_path = config_path;
}

void bs_adapter_attach_config_testing_clear_sync_fail_path(void)
{
    g_sync_fail_path = nullptr;
}
