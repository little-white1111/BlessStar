#include "bs/kernel/common/MemoryPool.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static void test_BsMemoryPool_CreateDestroy()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);
    assert(pool != nullptr);
    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_CreateDestroy: PASS\n");
}

static void test_BsMemoryPool_TinyAllocation()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr = bs_memory_pool_alloc(pool, 32);
    assert(ptr != nullptr);
    memset(ptr, 0xAA, 32);

    bs_memory_pool_free(pool, ptr);
    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_TinyAllocation: PASS\n");
}

static void test_BsMemoryPool_SmallAllocation()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr = bs_memory_pool_alloc(pool, 512);
    assert(ptr != nullptr);
    memset(ptr, 0xBB, 512);

    bs_memory_pool_free(pool, ptr);
    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_SmallAllocation: PASS\n");
}

static void test_BsMemoryPool_MediumAllocation()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr = bs_memory_pool_alloc(pool, 16 * 1024);
    assert(ptr != nullptr);
    memset(ptr, 0xCC, 16 * 1024);

    bs_memory_pool_free(pool, ptr);
    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_MediumAllocation: PASS\n");
}

static void test_BsMemoryPool_LargeAllocation()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr = bs_memory_pool_alloc(pool, 128 * 1024);
    assert(ptr != nullptr);
    memset(ptr, 0xDD, 128 * 1024);

    bs_memory_pool_free(pool, ptr);
    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_LargeAllocation: PASS\n");
}

static void test_BsMemoryPool_MultipleAllocations()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);
    const int     N    = 100;
    void*         ptrs[N];

    for (int i = 0; i < N; i++)
    {
        ptrs[i] = bs_memory_pool_alloc(pool, 32 + (i % 64));
        assert(ptrs[i] != nullptr);
    }

    for (int i = 0; i < N; i++)
    {
        bs_memory_pool_free(pool, ptrs[i]);
    }

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_MultipleAllocations: PASS\n");
}

static void test_BsMemoryPool_GetStats()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr1 = bs_memory_pool_alloc(pool, 32);
    void* ptr2 = bs_memory_pool_alloc(pool, 512);

    size_t allocated = bs_memory_pool_get_allocated(pool);
    size_t used      = bs_memory_pool_get_used(pool);

    assert(allocated > 0);
    assert(used > 0);

    bs_memory_pool_free(pool, ptr1);
    bs_memory_pool_free(pool, ptr2);

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_GetStats: PASS\n");
}

static void test_BsMemoryPool_Clear()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr1 = bs_memory_pool_alloc(pool, 32);
    void* ptr2 = bs_memory_pool_alloc(pool, 512);
    void* ptr3 = bs_memory_pool_alloc(pool, 16 * 1024);

    bs_memory_pool_clear(pool);

    assert(bs_memory_pool_get_allocated(pool) == 0);
    assert(bs_memory_pool_get_used(pool) == 0);

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_Clear: PASS\n");
}

static void test_BsMemoryPool_NullInput()
{
    void* ptr = bs_memory_pool_alloc(nullptr, 100);
    assert(ptr == nullptr);

    bs_memory_pool_free(nullptr, nullptr);
    bs_memory_pool_clear(nullptr);

    assert(bs_memory_pool_get_allocated(nullptr) == 0);
    assert(bs_memory_pool_get_used(nullptr) == 0);

    printf("test_BsMemoryPool_NullInput: PASS\n");
}

static void test_BsMemoryPool_Category()
{
    assert(bs_memory_pool_get_category(32) == BS_MEMORY_POOL_TINY);
    assert(bs_memory_pool_get_category(256) == BS_MEMORY_POOL_SMALL);
    assert(bs_memory_pool_get_category(16 * 1024) == BS_MEMORY_POOL_MEDIUM);
    assert(bs_memory_pool_get_category(128 * 1024) == BS_MEMORY_POOL_LARGE);
    printf("test_BsMemoryPool_Category: PASS\n");
}

int main()
{
    printf("=== BsMemoryPool Tests ===\n");
    test_BsMemoryPool_CreateDestroy();
    test_BsMemoryPool_TinyAllocation();
    test_BsMemoryPool_SmallAllocation();
    test_BsMemoryPool_MediumAllocation();
    test_BsMemoryPool_LargeAllocation();
    test_BsMemoryPool_MultipleAllocations();
    test_BsMemoryPool_GetStats();
    test_BsMemoryPool_Clear();
    test_BsMemoryPool_NullInput();
    test_BsMemoryPool_Category();
    printf("=== All BsMemoryPool Tests PASSED! ===\n");
    return 0;
}
