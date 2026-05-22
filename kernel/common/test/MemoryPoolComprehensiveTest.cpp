#include "bs/kernel/common/MemoryPool.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <thread>
#include <vector>

static void test_BsMemoryPool_AllCategories()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* tiny   = bs_memory_pool_alloc(pool, 32);
    void* small  = bs_memory_pool_alloc(pool, 512);
    void* medium = bs_memory_pool_alloc(pool, 16 * 1024);
    void* large  = bs_memory_pool_alloc(pool, 128 * 1024);

    assert(tiny != nullptr);
    assert(small != nullptr);
    assert(medium != nullptr);
    assert(large != nullptr);

    bs_memory_pool_free(pool, tiny);
    bs_memory_pool_free(pool, small);
    bs_memory_pool_free(pool, medium);
    bs_memory_pool_free(pool, large);

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_AllCategories: PASS\n");
}

static void test_BsMemoryPool_LargeAllocation()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr = bs_memory_pool_alloc(pool, 1024 * 1024);
    assert(ptr != nullptr);
    memset(ptr, 0xAA, 1024 * 1024);

    bs_memory_pool_free(pool, ptr);
    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_LargeAllocation: PASS\n");
}

static void test_BsMemoryPool_Stress()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);
    const int     N    = 10000;
    void*         ptrs[N];

    for (int i = 0; i < N; i++)
    {
        size_t size = 16 + (i % 1024);
        ptrs[i]     = bs_memory_pool_alloc(pool, size);
        assert(ptrs[i] != nullptr);
        memset(ptrs[i], i & 0xFF, size);
    }

    for (int i = 0; i < N; i++)
    {
        bs_memory_pool_free(pool, ptrs[i]);
    }

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_Stress: PASS\n");
}

static void test_BsMemoryPool_ThreadSafety()
{
    BsMemoryPool*            pool = bs_memory_pool_create(64, 1024, 64 * 1024);
    const int                N    = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; i++)
    {
        threads.emplace_back(
            [pool, N]()
            {
                for (int j = 0; j < N; j++)
                {
                    void* ptr = bs_memory_pool_alloc(pool, 32 + (j % 100));
                    if (ptr)
                    {
                        memset(ptr, 0, 32 + (j % 100));
                        bs_memory_pool_free(pool, ptr);
                    }
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_ThreadSafety: PASS\n");
}

static void test_BsMemoryPool_Stats()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* p1 = bs_memory_pool_alloc(pool, 32);
    void* p2 = bs_memory_pool_alloc(pool, 512);
    void* p3 = bs_memory_pool_alloc(pool, 16 * 1024);

    size_t allocated = bs_memory_pool_get_allocated(pool);
    size_t used      = bs_memory_pool_get_used(pool);

    assert(allocated > 0);
    assert(used > 0);

    bs_memory_pool_free(pool, p1);
    bs_memory_pool_free(pool, p2);
    bs_memory_pool_free(pool, p3);

    assert(bs_memory_pool_get_used(pool) < used);

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_Stats: PASS\n");
}

static void test_BsMemoryPool_Clear()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    for (int i = 0; i < 100; i++)
    {
        bs_memory_pool_alloc(pool, 32 + i);
    }

    bs_memory_pool_clear(pool);

    assert(bs_memory_pool_get_allocated(pool) == 0);
    assert(bs_memory_pool_get_used(pool) == 0);

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_Clear: PASS\n");
}

static void test_BsMemoryPool_SpecialValues()
{
    BsMemoryPool* pool = bs_memory_pool_create(64, 1024, 64 * 1024);

    void* ptr = bs_memory_pool_alloc(pool, 0);
    assert(ptr == nullptr);

    bs_memory_pool_free(pool, nullptr);
    bs_memory_pool_clear(nullptr);

    assert(bs_memory_pool_get_allocated(nullptr) == 0);
    assert(bs_memory_pool_get_used(nullptr) == 0);

    bs_memory_pool_destroy(pool);
    printf("test_BsMemoryPool_SpecialValues: PASS\n");
}

static void test_BsMemoryPool_Category()
{
    assert(bs_memory_pool_get_category(0) == BS_MEMORY_POOL_TINY);
    assert(bs_memory_pool_get_category(64) == BS_MEMORY_POOL_TINY);
    assert(bs_memory_pool_get_category(65) == BS_MEMORY_POOL_SMALL);
    assert(bs_memory_pool_get_category(1024) == BS_MEMORY_POOL_SMALL);
    assert(bs_memory_pool_get_category(1025) == BS_MEMORY_POOL_MEDIUM);
    assert(bs_memory_pool_get_category(64 * 1024) == BS_MEMORY_POOL_MEDIUM);
    assert(bs_memory_pool_get_category(64 * 1024 + 1) == BS_MEMORY_POOL_LARGE);
    printf("test_BsMemoryPool_Category: PASS\n");
}

int main()
{
    printf("=== BsMemoryPool Comprehensive Tests ===\n");
    test_BsMemoryPool_AllCategories();
    test_BsMemoryPool_LargeAllocation();
    test_BsMemoryPool_Stress();
    test_BsMemoryPool_ThreadSafety();
    test_BsMemoryPool_Stats();
    test_BsMemoryPool_Clear();
    test_BsMemoryPool_SpecialValues();
    test_BsMemoryPool_Category();
    printf("=== All BsMemoryPool Comprehensive Tests PASSED! ===\n");
    return 0;
}
