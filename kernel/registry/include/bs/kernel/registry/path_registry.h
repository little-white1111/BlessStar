#ifndef BS_KERNEL_REGISTRY_PATH_REGISTRY_H
#define BS_KERNEL_REGISTRY_PATH_REGISTRY_H

/*
 * C-ST-7 contract block:
 * Thread safety: PathRegistry not thread-safe unless externally synchronized.
 * Error semantics: Invalid paths rejected at register time with non-zero status.
 * Platform notes: Normalizes paths via path_normalize helpers.
 */

#include "bs/kernel/registry/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct PathRegistry PathRegistry;

    PathRegistry* bs_path_registry_create(void);
    void          bs_path_registry_destroy(PathRegistry* registry);

    int bs_path_registry_register_declaration(PathRegistry* registry, const char* path,
                                              const PathEntry* entry);
    int bs_path_registry_bind_instance(PathRegistry* registry, const char* path, void* impl);
    int bs_path_registry_resolve(PathRegistry* registry, const char* canonical_path, Binding* out);
    int bs_path_registry_unregister(PathRegistry* registry, const char* path);
    int bs_path_registry_freeze(PathRegistry* registry);
    int bs_path_registry_is_frozen(const PathRegistry* registry);

    RegistrationPhase bs_path_registry_current_phase(const PathRegistry* registry);

    /** Monotonic: P0 -> P1 -> P2 only (R-II-2 attach phases). */
    int bs_path_registry_advance_phase(PathRegistry* registry, RegistrationPhase phase);

    /**
     * Admin/test API: list immediate child path segments under prefix (max_depth <= 2).
     * out_paths: array of char* buffers each BS_REGISTRY_MAX_PATH (caller-provided).
     */
    int bs_path_registry_list_subtree(const PathRegistry* registry, const char* prefix,
                                      int max_depth, char** out_paths, int out_capacity,
                                      int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_REGISTRY_PATH_REGISTRY_H */
