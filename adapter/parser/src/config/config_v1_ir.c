#include "bs/kernel/ir/resolver.h"

#include "bs/adapter/parser/config_v1_ir.h"

#include <stdlib.h>
#include <string.h>

IRRequirementList* config_v1_build_manual_requirements(const ConfigV1Ast* ast)
{
    if (!ast || ast->manual_requirements_count == 0)
        return NULL;

    IRRequirementList* list = (IRRequirementList*)calloc(1, sizeof(IRRequirementList));
    if (!list)
        return NULL;

    for (size_t i = 0; i < ast->manual_requirements_count; ++i)
    {
        const char* type = ast->manual_requirements[i];
        if (!type || type[0] == '\0')
            continue;

        IRRequirementEntry* entry = (IRRequirementEntry*)calloc(1, sizeof(IRRequirementEntry));
        if (!entry)
        {
            bs_requirement_list_free(list);
            return NULL;
        }
        const size_t n   = strlen(type) + 1;
        char*        dup = (char*)malloc(n);
        if (!dup)
        {
            free(entry);
            bs_requirement_list_free(list);
            return NULL;
        }
        memcpy(dup, type, n);
        entry->instruction_type = dup;
        entry->next             = list->head;
        list->head              = entry;
        list->count++;
    }
    return list;
}

IRInstructionList* ir_generate_config_v1_from_ast(const ConfigV1Ast* ast)
{
    if (!ast)
        return NULL;

    IRInstructionList* list = ir_instruction_list_create();
    if (!list)
        return NULL;

    for (ConfigV1Instruction* it = ast->instructions; it; it = it->next)
    {
        if (!it->type || !it->name)
            continue;

        IRInstruction* instr = ir_instruction_create(it->type, it->name);
        if (!instr)
        {
            ir_instruction_list_destroy(list);
            return NULL;
        }

        for (ConfigV1Metadata* m = it->metadata; m; m = m->next)
        {
            if (!m->key || !m->value)
                continue;
            IRMetadata* meta = ir_metadata_create(m->key, m->value);
            if (meta)
                ir_instruction_add_metadata(instr, meta);
        }

        if (ir_instruction_list_add(list, instr) != 0)
        {
            ir_instruction_destroy(instr);
            ir_instruction_list_destroy(list);
            return NULL;
        }
    }

    return list;
}
