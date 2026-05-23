#ifndef BS_ADAPTER_PARSER_CONFIG_V1_IR_H
#define BS_ADAPTER_PARSER_CONFIG_V1_IR_H

#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/requirements.h"

#include "bs/adapter/parser/config_v1_ast.h"

#ifdef __cplusplus
extern "C"
{
#endif

    IRInstructionList* ir_generate_config_v1_from_ast(const ConfigV1Ast* ast);

    IRRequirementList* config_v1_build_manual_requirements(const ConfigV1Ast* ast);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_CONFIG_V1_IR_H */
