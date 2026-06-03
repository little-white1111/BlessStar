#ifndef BS_KERNEL_COMMON_MEMORY_POOL_H
#define BS_KERNEL_COMMON_MEMORY_POOL_H

/*
 * C-ST-7 contract block:
 * Thread safety: Pool not thread-safe unless documented per pool instance.
 * Error semantics: Alloc returns NULL on exhaustion; free tolerates NULL.
 * Platform notes: Optional arena for attach/reload hot paths.
 */

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C"
{
#else
#include <stddef.h>
#include <stdint.h>
#endif

    typedef enum BsMemoryPoolCategory
    {
        BS_MEMORY_POOL_TINY   = 1,
        BS_MEMORY_POOL_SMALL  = 2,
        BS_MEMORY_POOL_MEDIUM = 3,
        BS_MEMORY_POOL_LARGE  = 4
    } BsMemoryPoolCategory;

    typedef struct BsMemoryPool BsMemoryPool;

    BsMemoryPool* bs_memory_pool_create(size_t tiny_size, size_t small_size, size_t medium_size);

    void bs_memory_pool_destroy(BsMemoryPool* pool);

    void* bs_memory_pool_alloc(BsMemoryPool* pool, size_t size);

    void bs_memory_pool_free(BsMemoryPool* pool, void* ptr);

    void bs_memory_pool_clear(BsMemoryPool* pool);

    size_t bs_memory_pool_get_allocated(BsMemoryPool* pool);

    size_t bs_memory_pool_get_used(BsMemoryPool* pool);

    BsMemoryPoolCategory bs_memory_pool_get_category(size_t size);

#ifdef __cplusplus
}
#endif

#endif
