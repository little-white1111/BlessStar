#ifndef BS_ADAPTER_PARSER_CONFIG_V1_IR_H
#define BS_ADAPTER_PARSER_CONFIG_V1_IR_H

/*
 * C-ST-7 contract block:
 * Thread safety: pure on ast input; returned IR lists owned by caller.
 * Error semantics: null return on OOM or invalid ast; caller must not double-free.
 * Platform notes: N/A.
 */

#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/requirements.h"

#include "bs/adapter/parser/config_v1_ast.h"

#ifdef __cplusplus
extern "C"
{
#endif

    IRInstructionList* bs_config_v1_generate_ir_from_ast(const ConfigV1Ast* ast);

    IRRequirementList* bs_config_v1_build_manual_requirements(const ConfigV1Ast* ast);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_CONFIG_V1_IR_H */
