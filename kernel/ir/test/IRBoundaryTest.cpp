#include "bs/kernel/ir/ir.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <vector>

static void test_IR_NullInput()
{
    IRInstruction* instr = ir_instruction_create(nullptr, nullptr);
    assert(instr != nullptr);
    ir_instruction_destroy(instr);
    printf("test_IR_NullInput: PASS\n");
}

static void test_IR_EmptyStrings()
{
    IRInstruction* instr = ir_instruction_create("", "");
    assert(instr != nullptr);
    ir_instruction_destroy(instr);
    printf("test_IR_EmptyStrings: PASS\n");
}

static void test_IR_LongStrings()
{
    // Avoid ~1MiB on stack (default Windows thread stack is ~1MiB and overflows).
    std::vector<char> long_str(1024 * 1024);
    memset(long_str.data(), 'a', long_str.size() - 1);
    long_str[long_str.size() - 1] = '\0';

    IRInstruction* instr = ir_instruction_create(long_str.data(), long_str.data());
    assert(instr != nullptr);
    ir_instruction_destroy(instr);
    printf("test_IR_LongStrings: PASS\n");
}

static void test_IRMetadata_NullKey()
{
    IRInstruction* instr = ir_instruction_create("test", "name");
    IRMetadata*    meta  = ir_metadata_create(nullptr, "value");
    assert(meta != nullptr);
    ir_instruction_add_metadata(instr, meta);
    ir_instruction_destroy(instr);
    printf("test_IRMetadata_NullKey: PASS\n");
}

static void test_IRMetadata_DuplicateKey()
{
    IRInstruction* instr = ir_instruction_create("test", "name");
    IRMetadata*    meta1 = ir_metadata_create("key", "value1");
    IRMetadata*    meta2 = ir_metadata_create("key", "value2");
    ir_instruction_add_metadata(instr, meta1);
    ir_instruction_add_metadata(instr, meta2);
    ir_instruction_destroy(instr);
    printf("test_IRMetadata_DuplicateKey: PASS\n");
}

static void test_IRList_Empty()
{
    IRInstructionList* list = ir_instruction_list_create();
    assert(ir_instruction_list_size(list) == 0);
    assert(ir_instruction_list_get(list, 0) == nullptr);
    ir_instruction_list_destroy(list);
    printf("test_IRList_Empty: PASS\n");
}

static void test_IRList_LargeSize()
{
    IRInstructionList* list = ir_instruction_list_create();
    const int          N    = 100000;

    for (int i = 0; i < N; i++)
    {
        IRInstruction* instr = ir_instruction_create("type", "name");
        ir_instruction_list_add(list, instr);
    }

    assert(ir_instruction_list_size(list) == N);
    ir_instruction_list_destroy(list);
    printf("test_IRList_LargeSize: PASS\n");
}

static void test_IRList_IndexOutOfBounds()
{
    IRInstructionList* list  = ir_instruction_list_create();
    IRInstruction*     instr = ir_instruction_create("type", "name");
    ir_instruction_list_add(list, instr);

    assert(ir_instruction_list_get(list, -1) == nullptr);
    assert(ir_instruction_list_get(list, 1) == nullptr);
    assert(ir_instruction_list_get(list, 100) == nullptr);

    ir_instruction_list_destroy(list);
    printf("test_IRList_IndexOutOfBounds: PASS\n");
}

int main()
{
    printf("=== IR Boundary Tests ===\n");
    test_IR_NullInput();
    test_IR_EmptyStrings();
    test_IR_LongStrings();
    test_IRMetadata_NullKey();
    test_IRMetadata_DuplicateKey();
    test_IRList_Empty();
    test_IRList_LargeSize();
    test_IRList_IndexOutOfBounds();
    printf("=== All IR Boundary Tests PASSED! ===\n");
    return 0;
}
