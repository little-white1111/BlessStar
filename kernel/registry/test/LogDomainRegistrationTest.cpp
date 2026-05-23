#include "bs/kernel/io/io_status_table.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/registry/registry_status_table.h"

#include <cassert>

int main()
{
    RegistryFacade* facade = bs_registry_facade_create();
    assert(facade != nullptr);

    BsStatusDomainRegistration io_status{};
    io_status.domain_qname = "io";
    io_status.table        = k_io_status_table;
    io_status.table_len    = k_io_status_table_len;
    assert(bs_registry_facade_register_status_domain(facade, &io_status) == BS_REGISTRY_OK);

    BsLogDomainRegistration io_log{};
    io_log.domain_qname = "io";
    io_log.flags        = 0;
    assert(bs_registry_facade_register_log_domain(facade, &io_log) == BS_REGISTRY_OK);
    assert(bs_registry_facade_log_domain_id_by_qname(facade, "io") != 0);

    BsLogDomainRegistration orphan_log{};
    orphan_log.domain_qname = "missing";
    orphan_log.flags        = 0;
    assert(bs_registry_facade_register_log_domain(facade, &orphan_log) ==
           BS_REGISTRY_ERR_NOT_FOUND);

    assert(bs_registry_facade_freeze(facade) == BS_REGISTRY_OK);

    BsLogDomainRegistration late_log{};
    late_log.domain_qname = "io";
    late_log.flags        = 0;
    assert(bs_registry_facade_register_log_domain(facade, &late_log) == BS_REGISTRY_ERR_FROZEN);

    bs_registry_facade_destroy(facade);
    return 0;
}
