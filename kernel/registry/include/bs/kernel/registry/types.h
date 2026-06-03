#ifndef BS_KERNEL_REGISTRY_TYPES_H
#define BS_KERNEL_REGISTRY_TYPES_H

/*
 * C-ST-7 contract block:
 * Thread safety: POD descriptors; immutable after registration.
 * Error semantics: N/A (types only).
 * Platform notes: Shared structs for hub/facade/provider entries.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RegistryStatus
    {
        BS_REGISTRY_OK                 = 0,
        BS_REGISTRY_ERR_INVALID_PATH   = -1,
        BS_REGISTRY_ERR_NOT_FOUND      = -2,
        BS_REGISTRY_ERR_ALREADY_EXISTS = -3,
        BS_REGISTRY_ERR_FROZEN         = -4,
        BS_REGISTRY_ERR_MANIFEST       = -5,
        BS_REGISTRY_ERR_LOGICAL_ID     = -6,
        BS_REGISTRY_ERR_HUB_OVERRIDE   = -7,
        BS_REGISTRY_ERR_INVALID_ARG    = -8,
        BS_REGISTRY_ERR_NO_DECLARATION = -9,
        BS_REGISTRY_ERR_PHASE          = -10
    } RegistryStatus;

    typedef enum RegistrationPhase
    {
        BS_REGISTRY_PHASE_P0     = 0,
        BS_REGISTRY_PHASE_P1     = 1,
        BS_REGISTRY_PHASE_P2     = 2,
        BS_REGISTRY_PHASE_FROZEN = 3
    } RegistrationPhase;

    typedef enum PathEntrySource
    {
        BS_PATH_ENTRY_BUILTIN = 0,
        BS_PATH_ENTRY_PLUGIN  = 1
    } PathEntrySource;

    typedef struct PathEntry
    {
        PathEntrySource source;
        const char*     manifest_ref;
        const char*     type_constraint;
    } PathEntry;

    typedef struct Binding
    {
        void* impl;
    } Binding;

#define BS_REGISTRY_MAX_PATH 256
#define BS_REGISTRY_MAX_LOGICAL_ID 128
#define BS_REGISTRY_LIST_MAX_DEPTH 2

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_REGISTRY_TYPES_H */
