#ifndef BS_ADAPTER_REQUIREMENT_FILTER_H
#define BS_ADAPTER_REQUIREMENT_FILTER_H

#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/ir_gate.h"
#include "bs/kernel/ir/requirements.h"
#include "bs/kernel/ir/resolver.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Adapter-side bootstrap (once at attach): builtin contract must be structurally valid.
     * @return 0 ok, non-zero failure
     */
    int bs_adapter_requirement_filter_validate_builtin(void);

    /**
     * Adapter version must fall within [min,max] from builtin contract (MVP string compare).
     * @return 0 ok, non-zero incompatible
     */
    int bs_adapter_requirement_filter_check_adapter_version(const char* adapter_version);

    /**
     * Active requirement view = builtin union manual_extra (builtin wins on duplicate types).
     * Caller owns returned list and must call bs_requirement_list_free().
     */
    IRRequirementList* bs_adapter_requirement_filter_merge_activation(
        const IRRequirementList* manual_extra_requirements_or_null);

    /**
     * IR gate using an explicit active requirement list (typically output of merge_activation).
     */
    int
    bs_adapter_requirement_filter_verify_instructions(const IRInstructionList* list,
                                                      const IRRequirementList* active_requirements);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_REQUIREMENT_FILTER_H */
