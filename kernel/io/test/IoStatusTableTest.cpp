#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/io/io.h"
#include "bs/kernel/io/io_status_table.h"
#include "bs/kernel/registry/registry_facade.h"

#include <cassert>
#include <cstring>

int main()
{
    assert(bs_status_from_io(BS_IO_OK) == BS_STATUS_OK);
    const BsStatus t = bs_status_from_io(BS_IO_ERR_TIMEOUT);
    assert(bs_status_code(t) == 5);

    RegistryFacade*            facade = bs_registry_facade_create();
    uint16_t                   id     = 0;
    BsStatusDomainRegistration reg{};
    reg.domain_qname  = "io";
    reg.table         = k_io_status_table;
    reg.table_len     = k_io_status_table_len;
    reg.out_domain_id = &id;
    assert(bs_registry_facade_register_status_domain(facade, &reg) == BS_REGISTRY_OK);
    bs_io_status_set_domain_id(id);

    assert(bs_status_domain_id(bs_status_from_io(BS_IO_ERR_TIMEOUT)) == static_cast<int>(id));
    assert(bs_status_code(bs_status_from_io(BS_IO_ERR_TIMEOUT)) == 5);

    char           buf[64];
    const BsStatus encoded = bs_status_make(id, 5);
    assert(bs_status_format(encoded, facade, buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "io.TIMEOUT") == 0);

    bs_registry_facade_destroy(facade);
    return 0;
}
