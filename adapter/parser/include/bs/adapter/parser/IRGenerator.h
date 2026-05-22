#ifndef BS_ADAPTER_PARSER_IR_GENERATOR_H
#define BS_ADAPTER_PARSER_IR_GENERATOR_H

#include "bs/kernel/ir/ir.h"

#include "bs/adapter/parser/ASTNode.h"

#ifdef __cplusplus
extern "C"
{
#endif

    IRInstruction*     ir_generate_from_ast(const ASTNode* ast);
    IRInstructionList* ir_generate_list_from_ast(const ASTNode* ast);

    /** @deprecated for MVP main chain; use ir_generate_config_v1_from_ast (config_v1_ir.h). */

    int ir_validate_instruction(const IRInstruction* instr, char** error_message);

    int ir_version_compare(uint64_t version1, uint64_t version2);
    int ir_is_version_compatible(uint64_t instruction_version, uint64_t min_version,
                                 uint64_t max_version);

#ifdef __cplusplus
}
#endif

#endif // BS_ADAPTER_PARSER_IR_GENERATOR_H
