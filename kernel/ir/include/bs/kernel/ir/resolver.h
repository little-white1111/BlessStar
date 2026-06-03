#ifndef BS_KERNEL_IR_RESOLVER_H
#define BS_KERNEL_IR_RESOLVER_H

/*
 * C-ST-7 contract block:
 * Thread safety: Resolver tables are immutable after build.
 * Error semantics: Non-zero when requirement path cannot be resolved.
 * Platform notes: Resolves symbolic requirement names to registry paths.
 */

#include "bs/kernel/ir/requirements.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** Returns non-zero if list is structurally valid (non-null entries, non-empty types). */
    int bs_requirement_validate(const IRRequirementList* list);

    /**
     * Lexicographic MVP compare: adapter_version in [min, max] inclusive (string compare).
     * Empty min/max means no bound on that side.
     */
    int bs_requirement_check_compatibility(const char* kernel_version, const char* adapter_version,
                                           const char* min_adapter_version,
                                           const char* max_adapter_version);

    /**
     * Merge two lists by priority: higher `priority_wins` (1 = `a` wins on duplicate types).
     * Returns newly allocated list or NULL on OOM / invalid input. Caller must free via
     * bs_requirement_list_free.
     */
    IRRequirementList* bs_requirement_merge(const IRRequirementList* a, const IRRequirementList* b,
                                            int priority_a_wins);

    void bs_requirement_list_free(IRRequirementList* list);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_IR_RESOLVER_H */
