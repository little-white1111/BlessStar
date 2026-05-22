#include "bs/adapter/plugin/plugin_api.h"
#include "bs/adapter/plugin/plugin_ir_requirements.h"
#include "bs/adapter/plugin/plugin_manifest_paths.h"

#include "bs/kernel/io/io_status_table.h"
#include "bs/kernel/registry/registry_status_table.h"

int bs_adapter_plugin_log_domains_register(RegistryFacade* facade, AttachContext* ctx)
{
    (void)ctx;
    if (!facade)
        return -1;

    char path[512];
    if (bs_adapter_plugin_manifest_path("ir_requirements_log_domains.txt", path, sizeof(path)) == 0)
    {
        if (bs_adapter_plugin_validate_ir_requirements_ref(path) != 0)
            return -1;
    }

    uint16_t io_domain_id  = 0;
    uint16_t reg_domain_id = 0;

    BsStatusDomainRegistration io_status{};
    io_status.domain_qname  = "io";
    io_status.table         = k_io_status_table;
    io_status.table_len     = k_io_status_table_len;
    io_status.out_domain_id = &io_domain_id;
    if (bs_registry_facade_register_status_domain(facade, &io_status) != BS_REGISTRY_OK)
        return -1;

    BsStatusDomainRegistration reg_status{};
    reg_status.domain_qname  = "registry";
    reg_status.table         = k_registry_status_table;
    reg_status.table_len     = k_registry_status_table_len;
    reg_status.out_domain_id = &reg_domain_id;
    if (bs_registry_facade_register_status_domain(facade, &reg_status) != BS_REGISTRY_OK)
        return -1;

    BsLogDomainRegistration io_log{};
    io_log.domain_qname  = "io";
    io_log.flags         = 0;
    io_log.out_domain_id = nullptr;
    if (bs_registry_facade_register_log_domain(facade, &io_log) != BS_REGISTRY_OK)
        return -1;

    BsLogDomainRegistration reg_log{};
    reg_log.domain_qname  = "registry";
    reg_log.flags         = 0;
    reg_log.out_domain_id = nullptr;
    if (bs_registry_facade_register_log_domain(facade, &reg_log) != BS_REGISTRY_OK)
        return -1;

    bs_io_status_set_domain_id(io_domain_id);
    bs_registry_status_set_domain_id(reg_domain_id);
    return 0;
}
