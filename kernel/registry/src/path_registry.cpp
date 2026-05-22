#include "bs/kernel/registry/path_registry.h"

#include "bs/kernel/registry/path_normalize.h"

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct PathNode
{
    bool     has_declaration = false;
    PathEntry entry{};
    bool     has_binding   = false;
    Binding  binding{};
};

struct PathRegistry
{
    std::unordered_map<std::string, PathNode> nodes;
    mutable std::mutex                        mutex;
    RegistrationPhase                         phase = BS_REGISTRY_PHASE_P0;
    bool                                      frozen = false;
};

PathRegistry* bs_path_registry_create(void)
{
    return new PathRegistry();
}

void bs_path_registry_destroy(PathRegistry* registry)
{
    delete registry;
}

static int normalize_into(PathRegistry* registry, const char* path, char* buf, size_t buf_size)
{
    (void)registry;
    const int rc = bs_registry_normalize_path(path, buf, buf_size);
    if (rc != BS_REGISTRY_OK)
        return rc;
    if (!bs_registry_path_has_allowed_root(buf))
        return BS_REGISTRY_ERR_INVALID_PATH;
    return BS_REGISTRY_OK;
}

static int phase_allows_kernel_write(RegistrationPhase phase)
{
    return phase == BS_REGISTRY_PHASE_P1 || phase == BS_REGISTRY_PHASE_P2;
}

static int phase_allows_adapter_write(RegistrationPhase phase)
{
    return phase == BS_REGISTRY_PHASE_P2;
}

int bs_path_registry_register_declaration(PathRegistry* registry, const char* path,
                                          const PathEntry* entry)
{
    if (!registry || !path || !entry)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char norm[BS_REGISTRY_MAX_PATH];
    const int nrc = normalize_into(registry, path, norm, sizeof(norm));
    if (nrc != BS_REGISTRY_OK)
        return nrc;

    std::lock_guard<std::mutex> lock(registry->mutex);
    if (registry->frozen)
        return BS_REGISTRY_ERR_FROZEN;

    if (std::strncmp(norm, "/kernel/", 8) == 0)
    {
        if (!phase_allows_kernel_write(registry->phase))
            return BS_REGISTRY_ERR_PHASE;
        if (entry->source != BS_PATH_ENTRY_BUILTIN)
            return BS_REGISTRY_ERR_MANIFEST;
    }
    if (std::strncmp(norm, "/adapter/", 9) == 0)
    {
        if (!phase_allows_adapter_write(registry->phase))
            return BS_REGISTRY_ERR_PHASE;
        if (entry->source != BS_PATH_ENTRY_PLUGIN)
            return BS_REGISTRY_ERR_MANIFEST;
        if (!entry->manifest_ref || entry->manifest_ref[0] == '\0')
            return BS_REGISTRY_ERR_MANIFEST;
    }

    PathNode& node = registry->nodes[norm];
    if (node.has_declaration)
        return BS_REGISTRY_ERR_ALREADY_EXISTS;

    node.has_declaration     = true;
    node.entry               = *entry;
    node.entry.manifest_ref  = entry->manifest_ref;
    node.entry.type_constraint = entry->type_constraint;
    return BS_REGISTRY_OK;
}

int bs_path_registry_bind_instance(PathRegistry* registry, const char* path, void* impl)
{
    if (!registry || !path)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char norm[BS_REGISTRY_MAX_PATH];
    const int nrc = normalize_into(registry, path, norm, sizeof(norm));
    if (nrc != BS_REGISTRY_OK)
        return nrc;

    std::lock_guard<std::mutex> lock(registry->mutex);
    if (registry->frozen)
        return BS_REGISTRY_ERR_FROZEN;

    if (std::strncmp(norm, "/kernel/", 8) == 0 && !phase_allows_kernel_write(registry->phase))
        return BS_REGISTRY_ERR_PHASE;
    if (std::strncmp(norm, "/adapter/", 9) == 0 && !phase_allows_adapter_write(registry->phase))
        return BS_REGISTRY_ERR_PHASE;

    auto it = registry->nodes.find(norm);
    if (it == registry->nodes.end() || !it->second.has_declaration)
        return BS_REGISTRY_ERR_NO_DECLARATION;
    if (it->second.has_binding)
        return BS_REGISTRY_ERR_ALREADY_EXISTS;

    it->second.has_binding = true;
    it->second.binding.impl = impl;
    return BS_REGISTRY_OK;
}

int bs_path_registry_resolve(PathRegistry* registry, const char* canonical_path, Binding* out)
{
    if (!registry || !canonical_path || !out)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char norm[BS_REGISTRY_MAX_PATH];
    const int nrc = normalize_into(registry, canonical_path, norm, sizeof(norm));
    if (nrc != BS_REGISTRY_OK)
        return nrc;

    std::lock_guard<std::mutex> lock(registry->mutex);
    auto it = registry->nodes.find(norm);
    if (it == registry->nodes.end() || !it->second.has_binding)
        return BS_REGISTRY_ERR_NOT_FOUND;

    *out = it->second.binding;
    return BS_REGISTRY_OK;
}

int bs_path_registry_unregister(PathRegistry* registry, const char* path)
{
    if (!registry || !path)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char norm[BS_REGISTRY_MAX_PATH];
    const int nrc = normalize_into(registry, path, norm, sizeof(norm));
    if (nrc != BS_REGISTRY_OK)
        return nrc;

    std::lock_guard<std::mutex> lock(registry->mutex);
    if (registry->frozen)
        return BS_REGISTRY_ERR_FROZEN;

    const auto it = registry->nodes.find(norm);
    if (it == registry->nodes.end())
        return BS_REGISTRY_ERR_NOT_FOUND;
    registry->nodes.erase(it);
    return BS_REGISTRY_OK;
}

int bs_path_registry_freeze(PathRegistry* registry)
{
    if (!registry)
        return BS_REGISTRY_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> lock(registry->mutex);
    registry->frozen = true;
    registry->phase  = BS_REGISTRY_PHASE_FROZEN;
    return BS_REGISTRY_OK;
}

int bs_path_registry_is_frozen(const PathRegistry* registry)
{
    if (!registry)
        return 0;
    std::lock_guard<std::mutex> lock(registry->mutex);
    return registry->frozen ? 1 : 0;
}

RegistrationPhase bs_path_registry_current_phase(const PathRegistry* registry)
{
    if (!registry)
        return BS_REGISTRY_PHASE_P0;
    std::lock_guard<std::mutex> lock(registry->mutex);
    return registry->phase;
}

int bs_path_registry_advance_phase(PathRegistry* registry, RegistrationPhase phase)
{
    if (!registry)
        return BS_REGISTRY_ERR_INVALID_ARG;
    if (phase != BS_REGISTRY_PHASE_P1 && phase != BS_REGISTRY_PHASE_P2)
        return BS_REGISTRY_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(registry->mutex);
    if (registry->frozen)
        return BS_REGISTRY_ERR_FROZEN;

    const RegistrationPhase expected =
        (phase == BS_REGISTRY_PHASE_P1) ? BS_REGISTRY_PHASE_P0 : BS_REGISTRY_PHASE_P1;
    if (registry->phase != expected)
        return BS_REGISTRY_ERR_PHASE;

    registry->phase = phase;
    return BS_REGISTRY_OK;
}

int bs_path_registry_list_subtree(const PathRegistry* registry, const char* prefix, int max_depth,
                                  char** out_paths, int out_capacity, int* out_count)
{
    if (!registry || !prefix || !out_paths || !out_count || max_depth < 0 ||
        max_depth > BS_REGISTRY_LIST_MAX_DEPTH)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char norm_prefix[BS_REGISTRY_MAX_PATH];
    const int nrc = bs_registry_normalize_path(prefix, norm_prefix, sizeof(norm_prefix));
    if (nrc != BS_REGISTRY_OK)
        return nrc;

    const size_t prefix_len = std::strlen(norm_prefix);
    std::lock_guard<std::mutex> lock(registry->mutex);

    int count = 0;
    for (const auto& kv : registry->nodes)
    {
        if (count >= out_capacity)
            break;
        const std::string& p = kv.first;
        if (p.size() < prefix_len || p.compare(0, prefix_len, norm_prefix) != 0)
            continue;
        if (prefix_len > 0 && p.size() > prefix_len && p[prefix_len] != '/')
            continue;

        std::string rel = (prefix_len < p.size()) ? p.substr(prefix_len + (p[prefix_len] == '/' ? 1 : 0))
                                                  : std::string();
        if (rel.empty())
            continue;

        int slashes = 0;
        for (char c : rel)
            if (c == '/')
                ++slashes;
        if (slashes > max_depth)
            continue;

        std::strncpy(out_paths[count], p.c_str(), BS_REGISTRY_MAX_PATH - 1);
        out_paths[count][BS_REGISTRY_MAX_PATH - 1] = '\0';
        ++count;
    }
    *out_count = count;
    return BS_REGISTRY_OK;
}
