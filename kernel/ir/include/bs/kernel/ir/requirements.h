#ifndef BS_KERNEL_IR_REQUIREMENTS_H
#define BS_KERNEL_IR_REQUIREMENTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Single allowed IR instruction `type` string (see `IRInstruction::type`). */
    typedef struct IRRequirementEntry IRRequirementEntry;
    struct IRRequirementEntry
    {
        const char*         instruction_type;
        IRRequirementEntry* next;
    };

    /** Linked list of required instruction types (builtin or merged activation view). */
    typedef struct IRRequirementList IRRequirementList;
    struct IRRequirementList
    {
        IRRequirementEntry* head;
        size_t              count;
    };

    /** Built-in kernel contract (compile-time truth source for adapter bootstrap). */
    typedef struct KernelBuiltinRequirements
    {
        IRRequirementList requirements;
        char              kernel_version[32];
        char              min_adapter_version[32];
        char              max_adapter_version[32];
        char              release_notes[512];
    } KernelBuiltinRequirements;

    const KernelBuiltinRequirements* kernel_get_builtin_requirements(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_IR_REQUIREMENTS_H */
