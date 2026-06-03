#include "bs/kernel/ir/ir.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void test_IRInstruction_CreateDestroy()
{
    IRInstruction* instr = bs_ir_instruction_create("test", "name");
    assert(instr != nullptr);
    assert(instr->type != nullptr);
    assert(instr->name != nullptr);
    bs_ir_instruction_destroy(instr);
    printf("test_IRInstruction_CreateDestroy: PASS\n");
}

static void test_IRMetadata()
{
    IRInstruction* instr = bs_ir_instruction_create("test", "name");

    IRMetadata* meta1 = bs_ir_metadata_create("key1", "value1");
    IRMetadata* meta2 = bs_ir_metadata_create("key2", "value2");

    bs_ir_instruction_add_metadata(instr, meta1);
    bs_ir_instruction_add_metadata(instr, meta2);

    assert(strcmp(bs_ir_instruction_get_metadata(instr, "key1"), "value1") == 0);
    assert(strcmp(bs_ir_instruction_get_metadata(instr, "key2"), "value2") == 0);

    bs_ir_instruction_destroy(instr);
    printf("test_IRMetadata: PASS\n");
}

static void test_IRInstructionList()
{
    IRInstructionList* list = bs_ir_instruction_list_create();

    IRInstruction* instr1 = bs_ir_instruction_create("type1", "name1");
    IRInstruction* instr2 = bs_ir_instruction_create("type2", "name2");
    IRInstruction* instr3 = bs_ir_instruction_create("type3", "name3");

    bs_ir_instruction_list_add(list, instr1);
    bs_ir_instruction_list_add(list, instr2);
    bs_ir_instruction_list_add(list, instr3);

    assert(bs_ir_instruction_list_size(list) == 3);
    assert(strcmp(bs_ir_instruction_list_get(list, 0)->name, "name1") == 0);
    assert(strcmp(bs_ir_instruction_list_get(list, 1)->name, "name2") == 0);
    assert(strcmp(bs_ir_instruction_list_get(list, 2)->name, "name3") == 0);

    bs_ir_instruction_list_destroy(list);
    printf("test_IRInstructionList: PASS\n");
}

int main()
{
    printf("=== IR Tests ===\n");
    test_IRInstruction_CreateDestroy();
    test_IRMetadata();
    test_IRInstructionList();
    printf("=== All IR Tests PASSED! ===\n");
    return 0;
}
