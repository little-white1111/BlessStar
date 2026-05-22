#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/registry/registry_status_table.h"

#include <cassert>

int main()
{
    RegistryFacade* facade = bs_registry_facade_create();
    assert(facade != nullptr);

    BsStatusDomainRegistration io_reg{};
    io_reg.domain_qname = "registry";
    io_reg.table        = k_registry_status_table;
    io_reg.table_len    = k_registry_status_table_len;
    assert(bs_registry_facade_register_status_domain(facade, &io_reg) == BS_REGISTRY_OK);
    assert(bs_registry_facade_freeze(facade) == BS_REGISTRY_OK);

    BsStatusDomainRegistration late{};
    late.domain_qname = "late";
    late.table        = k_registry_status_table;
    late.table_len    = k_registry_status_table_len;
    assert(bs_registry_facade_register_status_domain(facade, &late) == BS_REGISTRY_ERR_FROZEN);

    bs_registry_facade_destroy(facade);
    return 0;
}
