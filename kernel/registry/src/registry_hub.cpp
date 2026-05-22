#include "bs/kernel/registry/registry_hub.h"

#include "bs/kernel/registry/path_normalize.h"

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

struct RegistryHub
{
    std::unordered_map<std::string, std::string> mappings;
    mutable std::mutex                           mutex;
    bool                                         frozen = false;
};

RegistryHub* bs_registry_hub_create(void)
{
    return new RegistryHub();
}

void bs_registry_hub_destroy(RegistryHub* hub)
{
    delete hub;
}

static int parse_logical_id(const char* logical_id, char* domain, size_t domain_size,
                            char* capability, size_t capability_size, char* name, size_t name_size)
{
    if (!logical_id || !domain || !capability || !name)
        return BS_REGISTRY_ERR_INVALID_ARG;

    int dot_count = 0;
    for (const char* p = logical_id; *p; ++p)
        if (*p == '.')
            ++dot_count;
    if (dot_count > 3)
        return BS_REGISTRY_ERR_LOGICAL_ID;

    const char* first_dot = std::strchr(logical_id, '.');
    if (!first_dot)
        return BS_REGISTRY_ERR_LOGICAL_ID;

    const char* last_dot = std::strrchr(logical_id, '.');
    if (!last_dot || last_dot == first_dot)
        return BS_REGISTRY_ERR_LOGICAL_ID;

    const size_t domain_len = static_cast<size_t>(first_dot - logical_id);
    if (domain_len == 0 || domain_len + 1 > domain_size)
        return BS_REGISTRY_ERR_LOGICAL_ID;
    std::memcpy(domain, logical_id, domain_len);
    domain[domain_len] = '\0';

    const char* cap_start = first_dot + 1;
    const size_t cap_len  = static_cast<size_t>(last_dot - cap_start);
    if (cap_len == 0 || cap_len + 1 > capability_size)
        return BS_REGISTRY_ERR_LOGICAL_ID;
    std::memcpy(capability, cap_start, cap_len);
    capability[cap_len] = '\0';

    const char* name_start = last_dot + 1;
    if (name_start[0] == '\0')
        return BS_REGISTRY_ERR_LOGICAL_ID;
    if (std::strlen(name_start) + 1 > name_size)
        return BS_REGISTRY_ERR_LOGICAL_ID;
    std::strcpy(name, name_start);

  /* <=3 hops: domain, capability, name */
    if (std::strchr(name, '.') != nullptr)
        return BS_REGISTRY_ERR_LOGICAL_ID;

    return BS_REGISTRY_OK;
}

int bs_registry_hub_register_mapping(RegistryHub* hub, const char* logical_id,
                                     const char* canonical_path, int allow_override)
{
    if (!hub || !logical_id || !canonical_path)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char domain[64], capability[96], name[64];
    if (parse_logical_id(logical_id, domain, sizeof(domain), capability, sizeof(capability),
                         name, sizeof(name)) != BS_REGISTRY_OK)
        return BS_REGISTRY_ERR_LOGICAL_ID;

    char norm[BS_REGISTRY_MAX_PATH];
    if (bs_registry_normalize_path(canonical_path, norm, sizeof(norm)) != BS_REGISTRY_OK)
        return BS_REGISTRY_ERR_INVALID_PATH;
    if (!bs_registry_path_has_allowed_root(norm))
        return BS_REGISTRY_ERR_INVALID_PATH;

    std::lock_guard<std::mutex> lock(hub->mutex);
    if (hub->frozen)
        return BS_REGISTRY_ERR_FROZEN;

    const auto it = hub->mappings.find(logical_id);
    if (it != hub->mappings.end())
    {
        if (!allow_override)
            return BS_REGISTRY_ERR_HUB_OVERRIDE;
        it->second = norm;
        return BS_REGISTRY_OK;
    }

    hub->mappings.emplace(logical_id, norm);
    return BS_REGISTRY_OK;
}

int bs_registry_hub_resolve(const RegistryHub* hub, const char* logical_id, char* out_canonical_path,
                            size_t out_size)
{
    if (!hub || !logical_id || !out_canonical_path || out_size == 0)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char domain[64], capability[96], name[64];
    if (parse_logical_id(logical_id, domain, sizeof(domain), capability, sizeof(capability),
                         name, sizeof(name)) != BS_REGISTRY_OK)
        return BS_REGISTRY_ERR_LOGICAL_ID;

    std::lock_guard<std::mutex> lock(hub->mutex);
    const auto it = hub->mappings.find(logical_id);
    if (it == hub->mappings.end())
        return BS_REGISTRY_ERR_NOT_FOUND;

    if (it->second.size() + 1 > out_size)
        return BS_REGISTRY_ERR_INVALID_ARG;
    std::strcpy(out_canonical_path, it->second.c_str());
    return BS_REGISTRY_OK;
}

int bs_registry_hub_is_frozen(const RegistryHub* hub)
{
    if (!hub)
        return 0;
    std::lock_guard<std::mutex> lock(hub->mutex);
    return hub->frozen ? 1 : 0;
}

int bs_registry_hub_freeze(RegistryHub* hub)
{
    if (!hub)
        return BS_REGISTRY_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> lock(hub->mutex);
    hub->frozen = true;
    return BS_REGISTRY_OK;
}
