#include "bs/kernel/ir/ir_gate.h"

#include <cstring>

static int type_allowed(const char* type, const IRRequirementList* requirements)
{
    if (!type || !requirements)
        return 0;
    for (IRRequirementEntry* e = requirements->head; e; e = e->next)
    {
        if (e->instruction_type && std::strcmp(e->instruction_type, type) == 0)
            return 1;
    }
    return 0;
}

extern "C" int bs_ir_gate_verify_instructions(const IRInstructionList* list,
                                              const IRRequirementList* requirements)
{
    if (!requirements)
        return -1;
    if (!list)
        return 0;
    const size_t n = ir_instruction_list_size(list);
    for (size_t i = 0; i < n; ++i)
    {
        const IRInstruction* instr = ir_instruction_list_get(list, i);
        if (!instr || !instr->type)
            return -1;
        if (!type_allowed(instr->type, requirements))
            return 1;
    }
    return 0;
}
