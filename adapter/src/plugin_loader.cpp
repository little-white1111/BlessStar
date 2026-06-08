#include "bs/adapter/attach_context.h"
#include "bs/adapter/plugin/attach_manifest_yaml.h"
#include "bs/adapter/plugin/plugin_api.h"
#include "bs/adapter/plugin/plugin_loader.h"
#include "bs/adapter/plugin/plugin_manifest_paths.h"

#include "bs/kernel/registry/registry_facade.h"

#include <cstdlib>
#include <cstring>

namespace
{

struct PluginDesc
{
    const char*               manifest_id;
    const char* const*        depends_on;
    int                       depends_count;
    int                       enabled_default;
    BsAdapterPluginRegisterFn register_fn;
};

struct PluginRuntime
{
    int         enabled;
    const char* depends_on[8];
    int         depends_count;
    const char* depends_storage[8];
};

static const char* k_no_deps[] = {nullptr};

static const char* k_io_deps[] = {"log-domains", nullptr};

static const char* k_orch_deps[] = {"log-domains", "io-standard", nullptr};

static int g_loaded_log_domains = 0;
static int g_loaded_io          = 0;
static int g_loaded_orch        = 0;

static PluginRuntime g_runtime[3];
static int           g_runtime_init = 0;

static const PluginDesc k_builtin_plugins[] = {
    {"log-domains", k_no_deps, 0, 1, bs_adapter_plugin_log_domains_register},
    {"io-standard", k_io_deps, 1, 1, bs_adapter_plugin_io_register},
    {"orch-reload", k_orch_deps, 2, 1, bs_adapter_plugin_orch_register},
};

static void init_runtime_defaults()
{
    if (g_runtime_init)
        return;
    g_runtime_init = 1;
    for (size_t i = 0; i < sizeof(k_builtin_plugins) / sizeof(k_builtin_plugins[0]); ++i)
    {
        g_runtime[i].enabled       = k_builtin_plugins[i].enabled_default;
        g_runtime[i].depends_count = k_builtin_plugins[i].depends_count;
        for (int d = 0; d < g_runtime[i].depends_count; ++d)
            g_runtime[i].depends_on[d] = k_builtin_plugins[i].depends_on[d];
    }
}

static const PluginDesc* find_plugin(const char* manifest_id)
{
    for (const auto& p : k_builtin_plugins)
    {
        if (std::strcmp(p.manifest_id, manifest_id) == 0)
            return &p;
    }
    return nullptr;
}

static int plugin_index(const char* manifest_id)
{
    for (size_t i = 0; i < sizeof(k_builtin_plugins) / sizeof(k_builtin_plugins[0]); ++i)
    {
        if (std::strcmp(k_builtin_plugins[i].manifest_id, manifest_id) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

static void apply_yaml_config_resolved(const char* resolved_path)
{
    if (!resolved_path)
        return;

    AttachManifestPluginConfig cfgs[8];
    const int                  n = bs_adapter_attach_manifest_yaml_load(resolved_path, cfgs, 8);
    if (n <= 0)
        return;

    for (int i = 0; i < n; ++i)
    {
        const int idx = plugin_index(cfgs[i].manifest_id);
        if (idx < 0)
            continue;
        if (cfgs[i].enabled >= 0)
            g_runtime[idx].enabled = cfgs[i].enabled;
        if (cfgs[i].depends_count > 0)
        {
            g_runtime[idx].depends_count = cfgs[i].depends_count;
            for (int d = 0; d < cfgs[i].depends_count; ++d)
            {
                if (g_runtime[idx].depends_storage[d])
                {
                    std::free((void*)g_runtime[idx].depends_storage[d]);
                    g_runtime[idx].depends_storage[d] = nullptr;
                }
                if (cfgs[i].depends_on[d])
                {
                    const size_t len = std::strlen(cfgs[i].depends_on[d]);
                    char*        dup = (char*)std::malloc(len + 1);
                    if (dup)
                    {
                        std::memcpy(dup, cfgs[i].depends_on[d], len);
                        dup[len]                          = '\0';
                        g_runtime[idx].depends_storage[d] = dup;
                        g_runtime[idx].depends_on[d]      = dup;
                    }
                }
            }
        }
    }

    bs_adapter_attach_manifest_yaml_free_configs(cfgs, n);
}

static int is_enabled(int idx)
{
    return idx >= 0 && g_runtime[idx].enabled != 0;
}

static int is_loaded(const char* manifest_id)
{
    if (std::strcmp(manifest_id, "log-domains") == 0)
        return g_loaded_log_domains;
    if (std::strcmp(manifest_id, "io-standard") == 0)
        return g_loaded_io;
    if (std::strcmp(manifest_id, "orch-reload") == 0)
        return g_loaded_orch;
    return 0;
}

static void mark_loaded(const char* manifest_id)
{
    if (std::strcmp(manifest_id, "log-domains") == 0)
        g_loaded_log_domains = 1;
    else if (std::strcmp(manifest_id, "io-standard") == 0)
        g_loaded_io = 1;
    else if (std::strcmp(manifest_id, "orch-reload") == 0)
        g_loaded_orch = 1;
}

static int deps_satisfied_idx(int idx)
{
    if (idx < 0)
        return 0;
    for (int i = 0; i < g_runtime[idx].depends_count; ++i)
    {
        const char* dep = g_runtime[idx].depends_on[i];
        if (!dep)
            break;
        if (!is_loaded(dep))
            return 0;
    }
    return 1;
}

/** Per-facade binding check (process-wide g_loaded_* skips re-init only). */
static int plugin_registered_on_facade(RegistryFacade* facade, const char* manifest_id)
{
    if (!facade || !manifest_id)
        return 0;
    if (std::strcmp(manifest_id, "io-standard") == 0)
    {
        Binding b{};
        return bs_registry_facade_resolve(facade, "/adapter/io/local", &b) == BS_REGISTRY_OK &&
               b.impl != nullptr;
    }
    if (std::strcmp(manifest_id, "log-domains") == 0)
        return bs_registry_facade_log_domain_id_by_qname(facade, "io") != 0;
    if (std::strcmp(manifest_id, "orch-reload") == 0)
    {
        Binding b{};
        return bs_registry_facade_resolve(facade, "/adapter/orchestration/reload_batch", &b) ==
                   BS_REGISTRY_OK &&
               b.impl != nullptr;
    }
    return 0;
}

static int ensure_phase_p2(RegistryFacade* facade)
{
    const RegistrationPhase phase = bs_registry_facade_current_phase(facade);
    if (phase == BS_REGISTRY_PHASE_FROZEN)
        return -1;
    if (phase == BS_REGISTRY_PHASE_P0)
        return -1;
    if (phase == BS_REGISTRY_PHASE_P1)
        return bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK
                   ? 0
                   : -1;
    if (phase == BS_REGISTRY_PHASE_P2)
        return 0;
    return -1;
}

static int invoke_plugin(AttachContext* ctx, const PluginDesc* plugin, int idx)
{
    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    if (!facade || !plugin || !plugin->register_fn)
        return -1;
    if (!is_enabled(idx))
        return 0;
    if (is_loaded(plugin->manifest_id) &&
        plugin_registered_on_facade(facade, plugin->manifest_id))
        return 0;
    if (!deps_satisfied_idx(idx))
        return -1;
    if (plugin->register_fn(facade, ctx) != 0)
        return -1;
    mark_loaded(plugin->manifest_id);
    return 0;
}

} // namespace

int bs_adapter_plugin_loader_load_one(AttachContext* ctx, const char* manifest_id)
{
    if (!ctx || !manifest_id)
        return -1;

    init_runtime_defaults();

    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    if (!facade)
        return -1;

    if (ensure_phase_p2(facade) != 0)
        return -1;

    const PluginDesc* plugin = find_plugin(manifest_id);
    const int         idx    = plugin_index(manifest_id);
    if (!plugin || idx < 0)
        return -1;

    if (!is_enabled(idx))
        return 0;

    if (std::strcmp(manifest_id, "io-standard") == 0 && !is_loaded("log-domains"))
    {
        if (bs_adapter_plugin_loader_load_one(ctx, "log-domains") != 0)
            return -1;
    }

    if (std::strcmp(manifest_id, "orch-reload") == 0)
    {
        if (!is_loaded("log-domains"))
        {
            if (bs_adapter_plugin_loader_load_one(ctx, "log-domains") != 0)
                return -1;
        }
        if (!is_loaded("io-standard"))
        {
            if (bs_adapter_plugin_loader_load_one(ctx, "io-standard") != 0)
                return -1;
        }
    }

    return invoke_plugin(ctx, plugin, idx);
}

int bs_adapter_plugin_loader_load_all(AttachContext* ctx, const char* attach_manifest_path)
{
    if (!ctx)
        return -1;

    init_runtime_defaults();

    {
        char        path[512];
        const char* src = attach_manifest_path ? attach_manifest_path : "attach_plugins.yaml";
        if (bs_adapter_plugin_manifest_path(src, path, sizeof(path)) == 0)
            apply_yaml_config_resolved(path);
    }

    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    if (!facade)
        return -1;

    if (ensure_phase_p2(facade) != 0)
        return -1;

    const int max_passes = 8;
    for (int pass = 0; pass < max_passes; ++pass)
    {
        int progressed = 0;
        for (size_t i = 0; i < sizeof(k_builtin_plugins) / sizeof(k_builtin_plugins[0]); ++i)
        {
            const auto& plugin = k_builtin_plugins[i];
            const int   idx    = static_cast<int>(i);
            if (!is_enabled(idx))
                continue;
            if (is_loaded(plugin.manifest_id) &&
                plugin_registered_on_facade(facade, plugin.manifest_id))
                continue;
            if (!deps_satisfied_idx(idx))
                continue;
            if (invoke_plugin(ctx, &plugin, idx) != 0)
                return -1;
            progressed = 1;
        }
        if (!progressed)
            break;
    }

    for (size_t i = 0; i < sizeof(k_builtin_plugins) / sizeof(k_builtin_plugins[0]); ++i)
    {
        const int idx = static_cast<int>(i);
        if (!is_enabled(idx))
            continue;
        if (!plugin_registered_on_facade(facade, k_builtin_plugins[i].manifest_id))
            return -1;
    }

    return 0;
}
