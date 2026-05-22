#ifndef BS_ADAPTER_PLUGIN_PLUGIN_LOADER_H
#define BS_ADAPTER_PLUGIN_PLUGIN_LOADER_H

#include "bs/adapter/attach_context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Load P2 plugins from attach manifest (IMPL-08-17).
     * @param attach_manifest_path NULL uses built-in table (synced with attach_plugins.yaml).
     * Advances P1 to P2 when needed; idempotent per manifest_id.
     */
    int bs_adapter_plugin_loader_load_all(AttachContext* ctx, const char* attach_manifest_path);

    /** Load one plugin by manifest_id (legacy register_standard_io maps to io-standard). */
    int bs_adapter_plugin_loader_load_one(AttachContext* ctx, const char* manifest_id);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PLUGIN_PLUGIN_LOADER_H */
