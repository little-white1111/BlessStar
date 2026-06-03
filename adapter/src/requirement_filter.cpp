#include "bs/adapter/requirement_filter.h"

int bs_adapter_requirement_filter_validate_builtin(void)
{
    const KernelBuiltinRequirements* k = bs_kernel_get_builtin_requirements();
    if (!k)
        return -1;
    return bs_requirement_validate(&k->requirements) ? 0 : -1;
}

int bs_adapter_requirement_filter_check_adapter_version(const char* adapter_version)
{
    const KernelBuiltinRequirements* k = bs_kernel_get_builtin_requirements();
    if (!k)
        return -1;
    return bs_requirement_check_compatibility(k->kernel_version, adapter_version,
                                              k->min_adapter_version, k->max_adapter_version)
               ? 0
               : 1;
}

IRRequirementList* bs_adapter_requirement_filter_merge_activation(
    const IRRequirementList* manual_extra_requirements_or_null)
{
    const KernelBuiltinRequirements* k = bs_kernel_get_builtin_requirements();
    if (!k)
        return nullptr;
    return bs_requirement_merge(&k->requirements, manual_extra_requirements_or_null, 1);
}

int bs_adapter_requirement_filter_verify_instructions(const IRInstructionList* list,
                                                      const IRRequirementList* active_requirements)
{
    return bs_ir_gate_verify_instructions(list, active_requirements);
}
