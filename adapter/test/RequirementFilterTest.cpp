#include "bs/kernel/ir/ir.h"

#include "bs/adapter/requirement_filter.h"

#include <cassert>
#include <cstring>

int main()
{
    assert(bs_adapter_requirement_filter_validate_builtin() == 0);
    assert(bs_adapter_requirement_filter_check_adapter_version("0.4.0") == 0);
    assert(bs_adapter_requirement_filter_check_adapter_version("0.0.1") != 0);

    IRRequirementList* active = bs_adapter_requirement_filter_merge_activation(nullptr);
    assert(active != nullptr);
    assert(active->count > 0u);

    IRInstructionList* list = bs_ir_instruction_list_create();
    IRInstruction*     ok   = bs_ir_instruction_create("type", "n");
    assert(bs_ir_instruction_list_add(list, ok) == 0);
    assert(bs_adapter_requirement_filter_verify_instructions(list, active) == 0);

    IRInstruction* bad = bs_ir_instruction_create("not_in_manifest", "n2");
    assert(bs_ir_instruction_list_add(list, bad) == 0);
    assert(bs_adapter_requirement_filter_verify_instructions(list, active) == 1);

    bs_ir_instruction_list_destroy(list);
    bs_requirement_list_free(active);
    return 0;
}
