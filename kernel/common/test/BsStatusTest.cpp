#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/io/io.h"
#include "bs/kernel/io/io_status_table.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/registry/registry_status_table.h"

#include <cassert>
#include <cstring>

int main()
{
    assert(BS_STATUS_DOMAIN_ENCODE_K == 1000);
    constexpr uint16_t kEncDomain = 7;
    constexpr int        kEncCode = 5;
    const BsStatus       enc      = bs_status_make(kEncDomain, kEncCode);
    assert(enc == -7005);
    assert(bs_status_domain_id(enc) == static_cast<int>(kEncDomain));
    assert(bs_status_code(enc) == kEncCode);

    RegistryFacade* facade = bs_registry_facade_create();
    assert(facade != nullptr);

    uint16_t reg_id = 0;
    uint16_t io_id  = 0;

    BsStatusDomainRegistration reg_reg{};
    reg_reg.domain_qname   = "registry";
    reg_reg.table          = k_registry_status_table;
    reg_reg.table_len      = k_registry_status_table_len;
    reg_reg.out_domain_id  = &reg_id;
    assert(bs_registry_facade_register_status_domain(facade, &reg_reg) == BS_REGISTRY_OK);

    BsStatusDomainRegistration io_reg{};
    io_reg.domain_qname   = "io";
    io_reg.table          = k_io_status_table;
    io_reg.table_len      = k_io_status_table_len;
    io_reg.out_domain_id  = &io_id;
    assert(bs_registry_facade_register_status_domain(facade, &io_reg) == BS_REGISTRY_OK);

    bs_io_status_set_domain_id(io_id);
    bs_registry_status_set_domain_id(reg_id);

    assert(bs_status_domain_id(bs_status_from_io(BS_IO_ERR_TIMEOUT)) == static_cast<int>(io_id));
    assert(bs_status_domain_id(bs_status_from_registry(BS_REGISTRY_ERR_PHASE)) ==
            static_cast<int>(reg_id));

    char buf[64];
    const BsStatus t = bs_status_make(io_id, 5);
    assert(bs_status_domain_id(t) == static_cast<int>(io_id));
    assert(bs_status_code(t) == 5);
    assert(bs_status_format(t, facade, buf, sizeof(buf)) == 0);
    assert(std::strcmp(buf, "io.TIMEOUT") == 0);

    bs_registry_facade_destroy(facade);
    return 0;
}
