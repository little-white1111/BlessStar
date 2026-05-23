#include "bs/kernel/registry/path_normalize.h"
#include "bs/kernel/registry/path_registry.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/registry/registry_hub.h"

#include <cstring>

#include <string>
#include <vector>

struct StatusDomainRecord
{
    std::string              qname;
    uint16_t                 domain_id;
    const BsStatusCodeEntry* table;
    size_t                   table_len;
};

struct LogDomainRecord
{
    std::string qname;
    uint16_t    domain_id;
    uint32_t    flags;
};

struct RegistryFacade
{
    PathRegistry*                   registry = nullptr;
    RegistryHub*                    hub      = nullptr;
    std::vector<StatusDomainRecord> status_domains;
    std::vector<LogDomainRecord>    log_domains;
    uint16_t                        next_domain_id = 1;
};

static int facade_frozen(const RegistryFacade* facade)
{
    return facade && bs_path_registry_current_phase(facade->registry) == BS_REGISTRY_PHASE_FROZEN;
}

static const StatusDomainRecord* find_status_by_id(const RegistryFacade* facade, int domain_id)
{
    if (!facade)
        return nullptr;
    for (const auto& d : facade->status_domains)
    {
        if (d.domain_id == static_cast<uint16_t>(domain_id))
            return &d;
    }
    return nullptr;
}

static const StatusDomainRecord* find_status_by_qname(const RegistryFacade* facade,
                                                      const char*           qname)
{
    if (!facade || !qname)
        return nullptr;
    for (const auto& d : facade->status_domains)
    {
        if (d.qname == qname)
            return &d;
    }
    return nullptr;
}

RegistryFacade* bs_registry_facade_create(void)
{
    auto* f     = new RegistryFacade();
    f->registry = bs_path_registry_create();
    f->hub      = bs_registry_hub_create();
    return f;
}

void bs_registry_facade_destroy(RegistryFacade* facade)
{
    if (!facade)
        return;
    bs_registry_hub_destroy(facade->hub);
    bs_path_registry_destroy(facade->registry);
    delete facade;
}

int bs_registry_facade_verify_manifest_ref(const char* path, const char* manifest_ref)
{
    if (!path)
        return BS_REGISTRY_ERR_INVALID_ARG;

    char norm[BS_REGISTRY_MAX_PATH];
    if (bs_registry_normalize_path(path, norm, sizeof(norm)) != BS_REGISTRY_OK)
        return BS_REGISTRY_ERR_INVALID_PATH;
    if (!bs_registry_path_has_allowed_root(norm))
        return BS_REGISTRY_ERR_INVALID_PATH;

    if (std::strncmp(norm, "/adapter/", 9) == 0)
    {
        if (!manifest_ref || manifest_ref[0] == '\0')
            return BS_REGISTRY_ERR_MANIFEST;
        return BS_REGISTRY_OK;
    }

    if (std::strncmp(norm, "/kernel/", 8) == 0)
        return BS_REGISTRY_OK;

    return BS_REGISTRY_ERR_INVALID_PATH;
}

int bs_registry_facade_register_declaration(RegistryFacade* facade, const char* path,
                                            const PathEntry* entry)
{
    if (!facade || !path || !entry)
        return BS_REGISTRY_ERR_INVALID_ARG;

    const int manifest_rc = bs_registry_facade_verify_manifest_ref(path, entry->manifest_ref);
    if (manifest_rc != BS_REGISTRY_OK)
        return manifest_rc;

    return bs_path_registry_register_declaration(facade->registry, path, entry);
}

int bs_registry_facade_register_hub_mapping(RegistryFacade* facade, const char* logical_id,
                                            const char* canonical_path, int allow_override)
{
    if (!facade)
        return BS_REGISTRY_ERR_INVALID_ARG;
    return bs_registry_hub_register_mapping(facade->hub, logical_id, canonical_path,
                                            allow_override);
}

RegistrationPhase bs_registry_facade_current_phase(const RegistryFacade* facade)
{
    if (!facade)
        return BS_REGISTRY_PHASE_P0;
    return bs_path_registry_current_phase(facade->registry);
}

int bs_registry_facade_advance_phase(RegistryFacade* facade, RegistrationPhase phase)
{
    if (!facade)
        return BS_REGISTRY_ERR_INVALID_ARG;
    return bs_path_registry_advance_phase(facade->registry, phase);
}

int bs_registry_facade_bind_instance(RegistryFacade* facade, const char* path, void* impl)
{
    if (!facade)
        return BS_REGISTRY_ERR_INVALID_ARG;
    return bs_path_registry_bind_instance(facade->registry, path, impl);
}

int bs_registry_facade_resolve(RegistryFacade* facade, const char* logical_id_or_path, Binding* out)
{
    if (!facade || !logical_id_or_path || !out)
        return BS_REGISTRY_ERR_INVALID_ARG;

    if (logical_id_or_path[0] == '/')
        return bs_path_registry_resolve(facade->registry, logical_id_or_path, out);

    char      canonical[BS_REGISTRY_MAX_PATH];
    const int hub_rc =
        bs_registry_hub_resolve(facade->hub, logical_id_or_path, canonical, sizeof(canonical));
    if (hub_rc != BS_REGISTRY_OK)
        return hub_rc;
    return bs_path_registry_resolve(facade->registry, canonical, out);
}

int bs_registry_facade_freeze(RegistryFacade* facade)
{
    if (!facade)
        return BS_REGISTRY_ERR_INVALID_ARG;
    const int pr = bs_path_registry_freeze(facade->registry);
    const int hr = bs_registry_hub_freeze(facade->hub);
    return (pr != BS_REGISTRY_OK) ? pr : hr;
}

uint64_t bs_registry_facade_snapshot_id(const RegistryFacade* facade)
{
    (void)facade;
    return 0;
}

int bs_registry_facade_register_status_domain(RegistryFacade*                   facade,
                                              const BsStatusDomainRegistration* reg)
{
    if (!facade || !reg || !reg->domain_qname || reg->domain_qname[0] == '\0' || !reg->table ||
        reg->table_len == 0)
        return BS_REGISTRY_ERR_INVALID_ARG;
    if (facade_frozen(facade))
        return BS_REGISTRY_ERR_FROZEN;
    if (find_status_by_qname(facade, reg->domain_qname))
        return BS_REGISTRY_ERR_ALREADY_EXISTS;

    StatusDomainRecord rec;
    rec.qname     = reg->domain_qname;
    rec.domain_id = facade->next_domain_id++;
    rec.table     = reg->table;
    rec.table_len = reg->table_len;
    facade->status_domains.push_back(rec);

    if (reg->out_domain_id)
        *reg->out_domain_id = rec.domain_id;
    return BS_REGISTRY_OK;
}

int bs_registry_facade_register_log_domain(RegistryFacade*                facade,
                                           const BsLogDomainRegistration* reg)
{
    if (!facade || !reg || !reg->domain_qname || reg->domain_qname[0] == '\0')
        return BS_REGISTRY_ERR_INVALID_ARG;
    if (facade_frozen(facade))
        return BS_REGISTRY_ERR_FROZEN;

    const StatusDomainRecord* status = find_status_by_qname(facade, reg->domain_qname);
    if (!status)
        return BS_REGISTRY_ERR_NOT_FOUND;

    for (const auto& existing : facade->log_domains)
    {
        if (existing.qname == reg->domain_qname)
            return BS_REGISTRY_ERR_ALREADY_EXISTS;
    }

    LogDomainRecord log_rec;
    log_rec.qname     = reg->domain_qname;
    log_rec.domain_id = status->domain_id;
    log_rec.flags     = reg->flags;
    facade->log_domains.push_back(log_rec);

    if (reg->out_domain_id)
        *reg->out_domain_id = log_rec.domain_id;
    return BS_REGISTRY_OK;
}

const char* bs_registry_facade_status_domain_qname(const RegistryFacade* facade, int domain_id)
{
    const StatusDomainRecord* rec = find_status_by_id(facade, domain_id);
    return rec ? rec->qname.c_str() : nullptr;
}

const char* bs_registry_facade_status_code_name(const RegistryFacade* facade, int domain_id,
                                                int code)
{
    const StatusDomainRecord* rec = find_status_by_id(facade, domain_id);
    if (!rec || code < 0)
        return nullptr;
    for (size_t i = 0; i < rec->table_len; ++i)
    {
        if (rec->table[i].code == code)
            return rec->table[i].name;
    }
    return nullptr;
}

uint16_t bs_registry_facade_log_domain_id_by_qname(const RegistryFacade* facade,
                                                   const char*           domain_qname)
{
    if (!facade || !domain_qname)
        return 0;
    for (const auto& d : facade->log_domains)
    {
        if (d.qname == domain_qname)
            return d.domain_id;
    }
    return 0;
}
