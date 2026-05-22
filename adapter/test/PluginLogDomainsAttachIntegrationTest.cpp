/**
 * IMPL-08-17 + day8: log-domains plugin registers io/registry status+log domains before freeze.
 */

#include "support/attach_test_fixture.h"

#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/io/io_status_table.h"
#include "bs/kernel/registry/registry_status_table.h"

#include <cstring>

int main()
{
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_attach_context_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);

    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);

    BsStatusDomainRegistration io_reg{};
    io_reg.domain_qname  = "io";
    io_reg.table         = k_io_status_table;
    io_reg.table_len     = k_io_status_table_len;
    uint16_t unused_id = 0;
    io_reg.out_domain_id = &unused_id;
    BS_TEST_REQUIRE("frozen", bs_registry_facade_register_status_domain(fix.facade, &io_reg) ==
                                   BS_REGISTRY_ERR_FROZEN);

    char buf[64];
    const BsStatus st = bs_status_make(1, 5);
    BS_TEST_REQUIRE("format", bs_status_format(st, fix.facade, buf, sizeof(buf)) == 0);
    BS_TEST_REQUIRE("format", std::strcmp(buf, "io.TIMEOUT") == 0);

    const BsStatus reg_st = bs_status_make(2, 10);
    BS_TEST_REQUIRE("format", bs_status_format(reg_st, fix.facade, buf, sizeof(buf)) == 0);
    BS_TEST_REQUIRE("format", std::strcmp(buf, "registry.PHASE") == 0);

    BsLogDomainRegistration late_log{};
    late_log.domain_qname = "io";
    late_log.flags        = 0;
    BS_TEST_REQUIRE("frozen", bs_registry_facade_register_log_domain(fix.facade, &late_log) ==
                                 BS_REGISTRY_ERR_FROZEN);

    bs_test_attach_teardown(&fix);
    return 0;
}
