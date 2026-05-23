#include "bs/kernel/common/MemoryPool.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

struct ChunkHeader
{
    size_t               size;
    BsMemoryPoolCategory category;
    bool                 is_allocated;
    ChunkHeader*         next;
    ChunkHeader*         prev;
};

struct BsMemoryPool
{
    std::mutex mutex;

    size_t tiny_size;
    size_t small_size;
    size_t medium_size;

    ChunkHeader* tiny_list;
    ChunkHeader* small_list;
    ChunkHeader* medium_list;

    std::vector<void*> large_blocks;

    std::atomic<size_t> total_allocated;
    std::atomic<size_t> total_used;

    std::unordered_map<void*, ChunkHeader*> allocation_map;
};

static const size_t DEFAULT_TINY_SIZE   = 64;
static const size_t DEFAULT_SMALL_SIZE  = 1024;
static const size_t DEFAULT_MEDIUM_SIZE = 64 * 1024;

BsMemoryPoolCategory bs_memory_pool_get_category(size_t size)
{
    if (size <= DEFAULT_TINY_SIZE)
        return BS_MEMORY_POOL_TINY;
    if (size <= DEFAULT_SMALL_SIZE)
        return BS_MEMORY_POOL_SMALL;
    if (size <= DEFAULT_MEDIUM_SIZE)
        return BS_MEMORY_POOL_MEDIUM;
    return BS_MEMORY_POOL_LARGE;
}

BsMemoryPool* bs_memory_pool_create(size_t tiny_size, size_t small_size, size_t medium_size)
{
    BsMemoryPool* pool = new BsMemoryPool();

    pool->tiny_size   = tiny_size > 0 ? tiny_size : DEFAULT_TINY_SIZE;
    pool->small_size  = small_size > 0 ? small_size : DEFAULT_SMALL_SIZE;
    pool->medium_size = medium_size > 0 ? medium_size : DEFAULT_MEDIUM_SIZE;

    pool->tiny_list   = nullptr;
    pool->small_list  = nullptr;
    pool->medium_list = nullptr;

    pool->total_allocated.store(0);
    pool->total_used.store(0);

    return pool;
}

void bs_memory_pool_destroy(BsMemoryPool* pool)
{
    if (!pool)
        return;

    {
        std::lock_guard<std::mutex> lock(pool->mutex);

        auto free_list = [](ChunkHeader*& list)
        {
            while (list)
            {
                ChunkHeader* next = list->next;
                free(list);
                list = next;
            }
        };

        free_list(pool->tiny_list);
        free_list(pool->small_list);
        free_list(pool->medium_list);

        for (void* block : pool->large_blocks)
        {
            ChunkHeader* header = reinterpret_cast<ChunkHeader*>(block) - 1;
            free(header);
        }
        pool->large_blocks.clear();
    }

    delete pool;
}

static void* allocate_block(size_t size)
{
    size_t       total_size = size + sizeof(ChunkHeader);
    ChunkHeader* chunk      = reinterpret_cast<ChunkHeader*>(malloc(total_size));
    if (!chunk)
        return nullptr;

    chunk->size         = size;
    chunk->category     = bs_memory_pool_get_category(size);
    chunk->is_allocated = true;
    chunk->next         = nullptr;
    chunk->prev         = nullptr;

    return reinterpret_cast<void*>(chunk + 1);
}

static ChunkHeader* allocate_chunk(BsMemoryPool* pool, size_t size)
{
    size_t       total_size = size + sizeof(ChunkHeader);
    ChunkHeader* chunk      = reinterpret_cast<ChunkHeader*>(malloc(total_size));
    if (!chunk)
        return nullptr;

    chunk->size         = size;
    chunk->category     = bs_memory_pool_get_category(size);
    chunk->is_allocated = false;
    chunk->next         = nullptr;
    chunk->prev         = nullptr;

    pool->total_allocated.fetch_add(total_size);

    return chunk;
}

void* bs_memory_pool_alloc(BsMemoryPool* pool, size_t size)
{
    if (!pool || size == 0)
        return nullptr;

    std::lock_guard<std::mutex> lock(pool->mutex);

    BsMemoryPoolCategory category = bs_memory_pool_get_category(size);

    ChunkHeader** list_ptr  = nullptr;
    size_t        pool_size = 0;

    switch (category)
    {
    case BS_MEMORY_POOL_TINY:
        list_ptr  = &pool->tiny_list;
        pool_size = pool->tiny_size;
        break;
    case BS_MEMORY_POOL_SMALL:
        list_ptr  = &pool->small_list;
        pool_size = pool->small_size;
        break;
    case BS_MEMORY_POOL_MEDIUM:
        list_ptr  = &pool->medium_list;
        pool_size = pool->medium_size;
        break;
    case BS_MEMORY_POOL_LARGE:
    {
        void* block = allocate_block(size);
        if (block)
        {
            ChunkHeader* header         = reinterpret_cast<ChunkHeader*>(block) - 1;
            pool->allocation_map[block] = header;
            pool->large_blocks.push_back(block);
            pool->total_used.fetch_add(size + sizeof(ChunkHeader));
        }
        return block;
    }
    default:
        return nullptr;
    }

    size = (size + 7) & ~7;
    if (size < pool_size)
    {
        size = pool_size;
    }

    ChunkHeader* current = *list_ptr;
    while (current)
    {
        if (!current->is_allocated && current->size >= size)
        {
            current->is_allocated = true;
            pool->total_used.fetch_add(current->size);
            void* ptr                 = reinterpret_cast<void*>(current + 1);
            pool->allocation_map[ptr] = current;
            return ptr;
        }
        current = current->next;
    }

    ChunkHeader* new_chunk = allocate_chunk(pool, size);
    if (!new_chunk)
        return nullptr;

    new_chunk->next = *list_ptr;
    if (*list_ptr)
    {
        (*list_ptr)->prev = new_chunk;
    }
    *list_ptr = new_chunk;

    new_chunk->is_allocated = true;
    pool->total_used.fetch_add(new_chunk->size);
    void* ptr                 = reinterpret_cast<void*>(new_chunk + 1);
    pool->allocation_map[ptr] = new_chunk;

    return ptr;
}

void bs_memory_pool_free(BsMemoryPool* pool, void* ptr)
{
    if (!pool || !ptr)
        return;

    std::lock_guard<std::mutex> lock(pool->mutex);

    auto it = pool->allocation_map.find(ptr);
    if (it == pool->allocation_map.end())
        return;

    ChunkHeader* header = it->second;
    pool->allocation_map.erase(it);

    if (header->category == BS_MEMORY_POOL_LARGE)
    {
        auto& large_blocks = pool->large_blocks;
        auto  block_it     = large_blocks.begin();
        while (block_it != large_blocks.end())
        {
            if (*block_it == ptr)
            {
                block_it = large_blocks.erase(block_it);
                break;
            }
            ++block_it;
        }
        pool->total_used.fetch_sub(header->size + sizeof(ChunkHeader));
        free(header);
        return;
    }

    header->is_allocated = false;
    pool->total_used.fetch_sub(header->size);
}

void bs_memory_pool_clear(BsMemoryPool* pool)
{
    if (!pool)
        return;

    std::lock_guard<std::mutex> lock(pool->mutex);

    auto clear_list =
        [](ChunkHeader*& list, std::atomic<size_t>& allocated, std::atomic<size_t>& used)
    {
        ChunkHeader* current = list;
        while (current)
        {
            ChunkHeader* next = current->next;
            if (current->is_allocated)
            {
                used.fetch_sub(current->size);
            }
            allocated.fetch_sub(current->size + sizeof(ChunkHeader));
            free(current);
            current = next;
        }
        list = nullptr;
    };

    clear_list(pool->tiny_list, pool->total_allocated, pool->total_used);
    clear_list(pool->small_list, pool->total_allocated, pool->total_used);
    clear_list(pool->medium_list, pool->total_allocated, pool->total_used);

    for (void* block : pool->large_blocks)
    {
        ChunkHeader* h = reinterpret_cast<ChunkHeader*>(block) - 1;
        pool->total_used.fetch_sub(h->size + sizeof(ChunkHeader));
        free(h);
    }
    pool->large_blocks.clear();
    pool->allocation_map.clear();
}

size_t bs_memory_pool_get_allocated(BsMemoryPool* pool)
{
    if (!pool)
        return 0;
    return pool->total_allocated.load();
}

size_t bs_memory_pool_get_used(BsMemoryPool* pool)
{
    if (!pool)
        return 0;
    return pool->total_used.load();
}
