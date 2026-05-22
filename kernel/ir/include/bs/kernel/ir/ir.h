#ifndef BS_KERNEL_IR_IR_H
#define BS_KERNEL_IR_IR_H

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

    IRInstruction* ir_instruction_create(const char* type, const char* name);
    void           ir_instruction_destroy(IRInstruction* instr);

    IRMetadata* ir_metadata_create(const char* key, const char* value);
    void        ir_metadata_destroy(IRMetadata* meta);

    void        ir_instruction_add_metadata(IRInstruction* instr, IRMetadata* meta);
    const char* ir_instruction_get_metadata(const IRInstruction* instr, const char* key);

    IRInstructionList* ir_instruction_list_create(void);
    void               ir_instruction_list_destroy(IRInstructionList* list);

    /**
     * Append an instruction to the list. On success, ownership transfers to the list:
     * do not call ir_instruction_destroy() on @p instr; ir_instruction_list_destroy() frees
     * all added instructions.
     * @return 0 success, -1 null list/instr, -2 allocation failure
     */
    int ir_instruction_list_add(IRInstructionList* list, IRInstruction* instr);

    size_t         ir_instruction_list_size(const IRInstructionList* list);
    IRInstruction* ir_instruction_list_get(const IRInstructionList* list, size_t index);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_IR_IR_H
