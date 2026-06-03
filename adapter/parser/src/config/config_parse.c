#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/parser/config_v1_ir.h"
#include "bs/adapter/parser/json_parser.h"
#include "bs/adapter/requirement_filter.h"

#include <string.h>

void bs_adapter_parser_result_destroy(BsConfigParseResult* result)
{
    if (!result)
        return;
    if (result->instructions)
        bs_ir_instruction_list_destroy(result->instructions);
    if (result->active_requirements)
        bs_requirement_list_free(result->active_requirements);
    result->instructions        = NULL;
    result->active_requirements = NULL;
    result->error_line          = 0;
    result->error_column        = 0;
}

BsStatus bs_adapter_parser_parse_bytes(const uint8_t* data, size_t len, BsConfigParseResult* out)
{
    if (!out)
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_INVALID_ARG);

    out->instructions        = NULL;
    out->active_requirements = NULL;
    out->error_line          = 0;
    out->error_column        = 0;

    if (!data && len > 0)
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_INVALID_ARG);

    ConfigV1Ast* ast = NULL;
    BsStatus     st =
        bs_json_parse_config_v1((const char*)data, len, &ast, &out->error_line, &out->error_column);
    if (!bs_status_is_ok(st))
        return st;

    const KernelBuiltinRequirements* builtin = bs_kernel_get_builtin_requirements();
    if (!builtin)
    {
        bs_config_v1_ast_destroy(ast);
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_PARSE);
    }

    if (!ast->kernel_version || strcmp(ast->kernel_version, builtin->kernel_version) != 0)
    {
        bs_config_v1_ast_destroy(ast);
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_KERNEL_VER);
    }

    if (!ast->adapter_version ||
        bs_adapter_requirement_filter_check_adapter_version(ast->adapter_version) != 0)
    {
        bs_config_v1_ast_destroy(ast);
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_ADAPTER_VER);
    }

    IRRequirementList* manual = bs_config_v1_build_manual_requirements(ast);
    if (ast->manual_requirements_count > 0 && !manual)
    {
        bs_config_v1_ast_destroy(ast);
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_OOM);
    }

    IRRequirementList* active = bs_adapter_requirement_filter_merge_activation(manual);
    if (manual)
        bs_requirement_list_free(manual);
    if (!active)
    {
        bs_config_v1_ast_destroy(ast);
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_OOM);
    }

    IRInstructionList* instructions = bs_config_v1_generate_ir_from_ast(ast);
    bs_config_v1_ast_destroy(ast);
    if (!instructions)
    {
        bs_requirement_list_free(active);
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_OOM);
    }

    out->instructions        = instructions;
    out->active_requirements = active;
    return BS_STATUS_OK;
}
