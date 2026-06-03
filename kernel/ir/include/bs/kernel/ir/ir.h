#ifndef BS_KERNEL_IR_IR_H
#define BS_KERNEL_IR_IR_H

/*
 * C-ST-7 contract block:
 * Thread safety: IR lists/instructions are not thread-safe unless externally locked.
 * Error semantics: NULL on alloc failure; destroy functions tolerate NULL.
 * Platform notes: Core IRInstruction graph; pairs with adapter config_v1_ir generator.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct IRMetadata        IRMetadata;
    typedef struct IRInstruction     IRInstruction;
    typedef struct IRInstructionList IRInstructionList;

    struct IRMetadata
    {
        const char* key;
        const char* value;
        IRMetadata* next;
    };

    struct IRInstruction
    {
        const char* type;
        const char* name;
        IRMetadata* metadata;
        uint64_t    version;
        uint64_t    timestamp;
    };

    IRInstruction* bs_ir_instruction_create(const char* type, const char* name);
    void           bs_ir_instruction_destroy(IRInstruction* instr);

    IRMetadata* bs_ir_metadata_create(const char* key, const char* value);
    void        bs_ir_metadata_destroy(IRMetadata* meta);

    void        bs_ir_instruction_add_metadata(IRInstruction* instr, IRMetadata* meta);
    const char* bs_ir_instruction_get_metadata(const IRInstruction* instr, const char* key);

    IRInstructionList* bs_ir_instruction_list_create(void);
    void               bs_ir_instruction_list_destroy(IRInstructionList* list);

    /**
     * Append an instruction to the list. On success, ownership transfers to the list:
     * do not call bs_ir_instruction_destroy() on @p instr; bs_ir_instruction_list_destroy() frees
     * all added instructions.
     * @return 0 success, -1 null list/instr, -2 allocation failure
     */
    int bs_ir_instruction_list_add(IRInstructionList* list, IRInstruction* instr);

    size_t         bs_ir_instruction_list_size(const IRInstructionList* list);
    IRInstruction* bs_ir_instruction_list_get(const IRInstructionList* list, size_t index);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_IR_IR_H
