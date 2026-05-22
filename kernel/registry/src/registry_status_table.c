#include "bs/kernel/registry/registry_status_table.h"

const BsStatusCodeEntry k_registry_status_table[] = {
    {0, "OK", 0},
    {1, "INVALID_PATH", 0},
    {2, "NOT_FOUND", 0},
    {3, "ALREADY_EXISTS", 0},
    {4, "FROZEN", 0},
    {5, "MANIFEST", 0},
    {6, "LOGICAL_ID", 0},
    {7, "HUB_OVERRIDE", 0},
    {8, "INVALID_ARG", 0},
    {9, "NO_DECLARATION", 0},
    {10, "PHASE", 0},
};

const size_t k_registry_status_table_len =
    sizeof(k_registry_status_table) / sizeof(k_registry_status_table[0]);

static uint16_t g_registry_status_domain_id = 2;

void bs_registry_status_set_domain_id(uint16_t domain_id)
{
    g_registry_status_domain_id = domain_id;
}

static int registry_status_to_code(int registry_status)
{
    switch (registry_status)
    {
    case BS_REGISTRY_OK:
        return 0;
    case BS_REGISTRY_ERR_INVALID_PATH:
        return 1;
    case BS_REGISTRY_ERR_NOT_FOUND:
        return 2;
    case BS_REGISTRY_ERR_ALREADY_EXISTS:
        return 3;
    case BS_REGISTRY_ERR_FROZEN:
        return 4;
    case BS_REGISTRY_ERR_MANIFEST:
        return 5;
    case BS_REGISTRY_ERR_LOGICAL_ID:
        return 6;
    case BS_REGISTRY_ERR_HUB_OVERRIDE:
        return 7;
    case BS_REGISTRY_ERR_INVALID_ARG:
        return 8;
    case BS_REGISTRY_ERR_NO_DECLARATION:
        return 9;
    case BS_REGISTRY_ERR_PHASE:
        return 10;
    default:
        return 8;
    }
}

BsStatus bs_status_from_registry(int registry_status)
{
    if (registry_status == BS_REGISTRY_OK)
        return BS_STATUS_OK;
    const int code = registry_status_to_code(registry_status);
    return bs_status_make(g_registry_status_domain_id, code);
}
