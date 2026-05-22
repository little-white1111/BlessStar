#include "bs/kernel/ir/ir.h"

#include "bs/adapter/parser/ASTNode.h"

#include <stdlib.h>
#include <string.h>

IRInstruction* ir_generate_from_ast(const ASTNode* ast)
{
    if (!ast)
        return NULL;

    const char* type = ast_node_type_to_string(ast->type);
    const char* name = ast->name ? ast->name : "unnamed";

    IRInstruction* instr = ir_instruction_create(type, name);
    if (!instr)
        return NULL;

    for (size_t i = 0; i < ast->child_count; i++)
    {
        const ASTNode* child = ast->children + i;

        if (child->name && child->value)
        {
            IRMetadata* meta = ir_metadata_create(child->name, child->value);
            if (meta)
            {
                ir_instruction_add_metadata(instr, meta);
            }
        }
    }

    return instr;
}

IRInstructionList* ir_generate_list_from_ast(const ASTNode* ast)
{
    if (!ast)
        return NULL;

    IRInstructionList* list = ir_instruction_list_create();
    if (!list)
        return NULL;

    for (size_t i = 0; i < ast->child_count; i++)
    {
        const ASTNode* child = ast->children + i;
        IRInstruction* instr = ir_generate_from_ast(child);

        if (instr)
        {
            ir_instruction_list_add(list, instr);
        }
    }

    return list;
}

int ir_validate_instruction(const IRInstruction* instr, char** error_message)
{
    if (!instr)
    {
        if (error_message)
        {
            *error_message = strdup("IR instruction is NULL");
        }
        return -1;
    }

    if (!instr->type || strlen(instr->type) == 0)
    {
        if (error_message)
        {
            *error_message = strdup("IR instruction type is empty");
        }
        return -1;
    }

    if (!instr->name || strlen(instr->name) == 0)
    {
        if (error_message)
        {
            *error_message = strdup("IR instruction name is empty");
        }
        return -1;
    }

    return 0;
}

int ir_version_compare(uint64_t version1, uint64_t version2)
{
    if (version1 < version2)
        return -1;
    if (version1 > version2)
        return 1;
    return 0;
}

int ir_is_version_compatible(uint64_t instruction_version, uint64_t min_version,
                             uint64_t max_version)
{
    return (instruction_version >= min_version) && (instruction_version <= max_version);
}
