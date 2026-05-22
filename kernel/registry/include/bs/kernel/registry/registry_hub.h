#ifndef BS_KERNEL_REGISTRY_REGISTRY_HUB_H
#define BS_KERNEL_REGISTRY_REGISTRY_HUB_H

#include "bs/kernel/registry/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RegistryHub RegistryHub;

    RegistryHub* bs_registry_hub_create(void);
    void         bs_registry_hub_destroy(RegistryHub* hub);

    /** allow_override: MVP default false (R-II-4 hub default deny override). */
    int bs_registry_hub_register_mapping(RegistryHub* hub, const char* logical_id,
                                         const char* canonical_path, int allow_override);

    int bs_registry_hub_resolve(const RegistryHub* hub, const char* logical_id,
                                char* out_canonical_path, size_t out_size);

    int bs_registry_hub_is_frozen(const RegistryHub* hub);
    int bs_registry_hub_freeze(RegistryHub* hub);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_REGISTRY_REGISTRY_HUB_H */
