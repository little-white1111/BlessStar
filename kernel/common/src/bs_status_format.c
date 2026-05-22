#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/registry/registry_facade.h"

#include <stdio.h>
#include <string.h>

int bs_status_format(BsStatus status, RegistryFacade* facade, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return -1;
    if (status == BS_STATUS_OK)
    {
        if (buf_len < 3)
            return -1;
        bs_safe_snprintf(buf, buf_len, "ok");
        return 0;
    }
    if (status > 0)
        return -1;

    const int domain_id = bs_status_domain_id(status);
    const int code      = bs_status_code(status);

    const char* qname = bs_registry_facade_status_domain_qname(facade, domain_id);
    const char* name  = bs_registry_facade_status_code_name(facade, domain_id, code);
    if (!qname || !name)
        return -1;

    if (bs_safe_snprintf(buf, buf_len, "%s.%s", qname, name) < 0 ||
        strnlen(buf, buf_len) >= buf_len)
        return -1;
    return 0;
}
