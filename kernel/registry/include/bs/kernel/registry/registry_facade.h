#ifndef BS_KERNEL_REGISTRY_REGISTRY_FACADE_H
#define BS_KERNEL_REGISTRY_REGISTRY_FACADE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Facade uses internal hub locking; see registry_facade.cpp.
 * Error semantics: BS_REGISTRY_ERR_* / BsStatus; no exceptions across extern "C" boundary.
 * Platform notes: Primary registry C ABI per ADR-BS-ABI-001.
 */

#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/registry/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RegistryFacade RegistryFacade;

    RegistryFacade* bs_registry_facade_create(void);
    void            bs_registry_facade_destroy(RegistryFacade* facade);

    int bs_registry_facade_register_declaration(RegistryFacade* facade, const char* path,
                                                const PathEntry* entry);
    int bs_registry_facade_register_hub_mapping(RegistryFacade* facade, const char* logical_id,
                                                const char* canonical_path, int allow_override);
    RegistrationPhase bs_registry_facade_current_phase(const RegistryFacade* facade);
    int bs_registry_facade_advance_phase(RegistryFacade* facade, RegistrationPhase phase);
    int bs_registry_facade_bind_instance(RegistryFacade* facade, const char* path, void* impl);
    int bs_registry_facade_resolve(RegistryFacade* facade, const char* logical_id_or_path,
                                   Binding* out);
    int bs_registry_facade_freeze(RegistryFacade* facade);
    int bs_registry_facade_verify_manifest_ref(const char* path, const char* manifest_ref);

    /** Scheme 3 placeholder: not implemented (returns 0). */
    uint64_t bs_registry_facade_snapshot_id(const RegistryFacade* facade);

    typedef struct BsLogDomainRegistration
    {
        const char* domain_qname;
        uint32_t    flags;
        uint16_t*   out_domain_id;
    } BsLogDomainRegistration;

    int bs_registry_facade_register_status_domain(RegistryFacade*                   facade,
                                                  const BsStatusDomainRegistration* reg);
    int bs_registry_facade_register_log_domain(RegistryFacade*                facade,
                                               const BsLogDomainRegistration* reg);

    const char* bs_registry_facade_status_domain_qname(const RegistryFacade* facade, int domain_id);
    const char* bs_registry_facade_status_code_name(const RegistryFacade* facade, int domain_id,
                                                    int code);
    uint16_t    bs_registry_facade_log_domain_id_by_qname(const RegistryFacade* facade,
                                                          const char*           domain_qname);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_REGISTRY_REGISTRY_FACADE_H */
