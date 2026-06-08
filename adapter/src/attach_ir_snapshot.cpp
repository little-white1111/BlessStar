#include "bs/kernel/ir/ir.h"

#include "bs/adapter/attach_ir_snapshot.h"

#include <cassert>
#include <cstring>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#include "attach_context_internal.h"

struct IrSnapshotEntry
{
    BsAttachIrSnapshotHandle handle = 0;
    std::string              path;
    uint64_t                 revision     = 0;
    IRInstructionList*       instructions = nullptr;
    uint32_t                 pin_count    = 0;
    int                      is_latest    = 0;
};

struct IrSnapshotStore
{
    std::mutex                   mu;
    uint64_t                     next_handle = 1;
    std::vector<IrSnapshotEntry> entries;
};

static IrSnapshotStore* snapshot_store(AttachContext* ctx)
{
    return ctx ? static_cast<IrSnapshotStore*>(ctx->ir_snapshot_store) : nullptr;
}

static IrSnapshotEntry* find_entry(IrSnapshotStore* store, BsAttachIrSnapshotHandle handle)
{
    if (!store || handle == 0)
        return nullptr;
    for (auto& entry : store->entries)
    {
        if (entry.handle == handle)
            return &entry;
    }
    return nullptr;
}

static void evict_unpinned_non_latest(IrSnapshotStore* store, const char* path)
{
    for (auto it = store->entries.begin(); it != store->entries.end();)
    {
        if (it->path == path && !it->is_latest && it->pin_count == 0)
        {
            if (it->instructions)
                bs_ir_instruction_list_destroy(it->instructions);
            it = store->entries.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void bs_adapter_attach_ir_snapshot_init(AttachContext* ctx)
{
    if (!ctx || ctx->ir_snapshot_store)
        return;
    ctx->ir_snapshot_store = new IrSnapshotStore();
}

void bs_adapter_attach_ir_snapshot_destroy(AttachContext* ctx)
{
    if (!ctx || !ctx->ir_snapshot_store)
        return;
    auto* store = static_cast<IrSnapshotStore*>(ctx->ir_snapshot_store);
    for (auto& entry : store->entries)
    {
        if (entry.instructions)
            bs_ir_instruction_list_destroy(entry.instructions);
    }
#ifndef NDEBUG
    for (const auto& entry : store->entries)
        assert(entry.pin_count == 0);
#endif
    delete store;
    ctx->ir_snapshot_store = nullptr;
}

BsAttachIrSnapshotHandle bs_adapter_attach_ir_snapshot_publish(AttachContext* ctx, const char* path,
                                                               uint64_t           revision,
                                                               IRInstructionList* instructions)
{
    if (!ctx || !path || !instructions)
        return 0;

    IrSnapshotStore* store = snapshot_store(ctx);
    if (!store)
        return 0;

    std::lock_guard<std::mutex> lock(store->mu);
    for (auto& entry : store->entries)
    {
        if (entry.path == path)
            entry.is_latest = 0;
    }

    IrSnapshotEntry entry{};
    entry.handle       = store->next_handle++;
    entry.path         = path;
    entry.revision     = revision;
    entry.instructions = instructions;
    entry.pin_count    = 0;
    entry.is_latest    = 1;
    store->entries.push_back(entry);

    evict_unpinned_non_latest(store, path);
    return entry.handle;
}

void bs_adapter_attach_ir_snapshot_pin(AttachContext* ctx, BsAttachIrSnapshotHandle handle)
{
    IrSnapshotStore* store = snapshot_store(ctx);
    if (!store)
        return;
    std::lock_guard<std::mutex> lock(store->mu);
    IrSnapshotEntry*            entry = find_entry(store, handle);
    if (entry)
        entry->pin_count++;
}

void bs_adapter_attach_ir_snapshot_unpin(AttachContext* ctx, BsAttachIrSnapshotHandle handle)
{
    IrSnapshotStore* store = snapshot_store(ctx);
    if (!store)
        return;
    std::lock_guard<std::mutex> lock(store->mu);
    IrSnapshotEntry*            entry = find_entry(store, handle);
    if (!entry || entry->pin_count == 0)
        return;
    entry->pin_count--;
    if (!entry->is_latest && entry->pin_count == 0)
    {
        if (entry->instructions)
            bs_ir_instruction_list_destroy(entry->instructions);
        const BsAttachIrSnapshotHandle doomed = entry->handle;
        store->entries.erase(std::remove_if(store->entries.begin(), store->entries.end(),
                                            [doomed](const IrSnapshotEntry& e)
                                            { return e.handle == doomed; }),
                             store->entries.end());
    }
}

IRInstructionList* bs_adapter_attach_ir_snapshot_instructions(AttachContext*           ctx,
                                                              BsAttachIrSnapshotHandle handle)
{
    IrSnapshotStore* store = snapshot_store(ctx);
    if (!store)
        return nullptr;
    std::lock_guard<std::mutex> lock(store->mu);
    IrSnapshotEntry*            entry = find_entry(store, handle);
    return entry ? entry->instructions : nullptr;
}

void bs_adapter_attach_ir_snapshot_clear_all(AttachContext* ctx)
{
    IrSnapshotStore* store = snapshot_store(ctx);
    if (!store)
        return;
    std::lock_guard<std::mutex> lock(store->mu);
    for (auto it = store->entries.begin(); it != store->entries.end();)
    {
        if (it->pin_count > 0)
        {
            ++it;
            continue;
        }
        if (it->instructions)
            bs_ir_instruction_list_destroy(it->instructions);
        it = store->entries.erase(it);
    }
}

size_t bs_adapter_attach_ir_snapshot_entry_count(AttachContext* ctx)
{
    IrSnapshotStore* store = snapshot_store(ctx);
    if (!store)
        return 0;
    std::lock_guard<std::mutex> lock(store->mu);
    return store->entries.size();
}
