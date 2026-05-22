#ifndef BS_KERNEL_REGISTRY_PATH_NORMALIZE_H
#define BS_KERNEL_REGISTRY_PATH_NORMALIZE_H

#include "bs/kernel/registry/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** Normalize path into out (must hold BS_REGISTRY_MAX_PATH). Returns BS_REGISTRY_OK or error. */
    int bs_registry_normalize_path(const char* in, char* out, size_t out_size);

    /** True if path is under /kernel or /adapter after normalization. */
    int bs_registry_path_has_allowed_root(const char* normalized_path);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_REGISTRY_PATH_NORMALIZE_H */
