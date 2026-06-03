#include "bs/adapter/parser/config_parse_status.h"

static uint16_t g_config_parse_domain_id = 60;

void bs_adapter_parser_status_set_domain_id(uint16_t domain_id)
{
    g_config_parse_domain_id = domain_id;
}

BsStatus bs_status_from_config_parse(int parse_err_code)
{
    if (parse_err_code == 0)
        return BS_STATUS_OK;
    return bs_status_make(g_config_parse_domain_id, parse_err_code);
}
