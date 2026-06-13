#include "bs/app/sdk/config_session_reader.h"

#include "bs/adapter/attach_context.h"

#include <cstdlib>
#include <cstring>

#include <utility>

namespace bs::app {

ConfigSessionReader::ConfigSessionReader(AttachContext* ctx)
    : ctx_(ctx)
    , last_version_(0)
{
}

ConfigSessionReader::~ConfigSessionReader()
{
    // 释放 LRU 缓存中所有深拷贝的 instruction
    for (auto& entry : list_)
        bs_ir_instruction_destroy(entry.value);
}

std::string ConfigSessionReader::make_key(const char* path_key, const char* instr_name) const
{
    std::string key;
    if (path_key)
        key.append(path_key);
    key.push_back(':');
    if (instr_name)
        key.append(instr_name);
    return key;
}

void ConfigSessionReader::evict_one_locked()
{
    if (list_.empty())
        return;

    // 淘汰链表尾部（最近最少使用）
    auto tail = std::prev(list_.end());
    map_.erase(tail->key);
    bs_ir_instruction_destroy(tail->value);
    list_.pop_back();
}

/* static */ IRInstruction* ConfigSessionReader::clone_instruction(const IRInstruction* src)
{
    if (!src)
        return nullptr;

    IRInstruction* dst = bs_ir_instruction_create(src->type, src->name);
    if (!dst)
        return nullptr;

    dst->version   = src->version;
    dst->timestamp = src->timestamp;

    // 深拷贝 metadata 链
    IRMetadata* src_meta = src->metadata;
    IRMetadata* tail     = nullptr;
    while (src_meta)
    {
        IRMetadata* meta_copy = bs_ir_metadata_create(src_meta->key, src_meta->value);
        if (!meta_copy)
        {
            bs_ir_instruction_destroy(dst);
            return nullptr;
        }

        // 追加到链表尾部（保持原序）
        if (!tail)
            dst->metadata = meta_copy;
        else
            tail->next = meta_copy;
        tail     = meta_copy;
        src_meta = src_meta->next;
    }

    return dst;
}

void ConfigSessionReader::Refresh()
{
    std::lock_guard<std::mutex> lock(mtx_);

    // 清空 LRU 缓存
    for (auto& entry : list_)
        bs_ir_instruction_destroy(entry.value);
    list_.clear();
    map_.clear();

    // 重读版本号
    last_version_ = bs_adapter_attach_ctx_get_hot_update_version(ctx_);
}

const IRInstruction* ConfigSessionReader::GetInstruction(const char* path_key,
                                                         const char* instr_name)
{
    if (!ctx_ || !path_key || !instr_name)
        return nullptr;

    std::lock_guard<std::mutex> lock(mtx_);

    // 版本号比对：版本递增时自动 Refresh
    const uint64_t current_version = bs_adapter_attach_ctx_get_hot_update_version(ctx_);
    if (current_version > last_version_)
    {
        for (auto& entry : list_)
            bs_ir_instruction_destroy(entry.value);
        list_.clear();
        map_.clear();
        last_version_ = current_version;
    }

    const std::string key = make_key(path_key, instr_name);

    // 查 LRU 缓存
    auto it = map_.find(key);
    if (it != map_.end())
    {
        // Hit: 移到链表头部（最近使用）
        list_.splice(list_.begin(), list_, it->second);
        return it->second->value;
    }

    // Miss: 查 gate_cache
    const IRInstructionList* gate_list =
        bs_adapter_attach_ctx_get_gate_result(ctx_, path_key);
    if (!gate_list)
        return nullptr;

    // 遍历查找匹配 name 的指令
    IRInstruction* matched = nullptr;
    const size_t   n      = bs_ir_instruction_list_size(gate_list);
    for (size_t i = 0; i < n; ++i)
    {
        IRInstruction* instr = bs_ir_instruction_list_get(gate_list, i);
        if (instr && instr->name && std::strcmp(instr->name, instr_name) == 0)
        {
            matched = instr;
            break;
        }
    }
    if (!matched)
        return nullptr;

    // 深拷贝到 LRU 缓存
    IRInstruction* cloned = clone_instruction(matched);
    if (!cloned)
        return nullptr;

    // LRU 检查软上限
    if (list_.size() >= kSoftLimit)
        evict_one_locked();

    Entry entry;
    entry.key   = key;
    entry.value = cloned;

    map_[key] = list_.emplace(list_.begin(), std::move(entry));

    return list_.front().value;
}

const IRMetadata* ConfigSessionReader::GetMetadata(const char* path_key, const char* instr_name)
{
    const IRInstruction* instr = GetInstruction(path_key, instr_name);
    return instr ? instr->metadata : nullptr;
}

} // namespace bs::app
