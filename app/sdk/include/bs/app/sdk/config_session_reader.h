#ifndef BS_APP_SDK_CONFIG_SESSION_READER_H
#define BS_APP_SDK_CONFIG_SESSION_READER_H

/*
 * MD-D-01: ConfigSessionReader — 结构化消费 instructions.metadata
 *
 * 从 AttachContext* 构造，通过 Gate 缓存（gate_cache）获取 IRInstructionList，
 * 在内部 LRU 缓存中保存深拷贝的 IRInstruction，App 开发者无需手动 JSON 解析。
 *
 * 线程安全：mtx_ 保护所有成员访问。
 */

#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

#include "bs/kernel/ir/ir.h"

typedef struct AttachContext AttachContext;

namespace bs::app {

class ConfigSessionReader {
public:
    /** 从 AttachContext* 构造。 */
    explicit ConfigSessionReader(AttachContext* ctx);
    ~ConfigSessionReader();

    /** 非拷贝 / 非移动。 */
    ConfigSessionReader(const ConfigSessionReader&)            = delete;
    ConfigSessionReader& operator=(const ConfigSessionReader&) = delete;

    /**
     * 获取指定路径+指令名的 IRInstruction。
     * 内部流程：持有锁 → 版本号比对 → 查LRU缓存(hit→返回) → 查gate_cache(miss→返回null) → 深拷贝回填LRU
     * 返回 const 指针，生命周期由 LRU 缓存保活（直到被淘汰或被 Refresh 清空）。
     */
    const IRInstruction* GetInstruction(const char* path_key, const char* instr_name);

    /**
     * 等价于 GetInstruction(...)?.metadata。
     * 方便快捷获取 metadata 链头。
     */
    const IRMetadata* GetMetadata(const char* path_key, const char* instr_name);

    /** 清空 LRU 缓存 + 重读版本号。 */
    void Refresh();

private:
    AttachContext* ctx_;
    uint64_t       last_version_ = 0;

    // LRU 缓存：<list<Entry>, map<key, iter>>。Entry.value 是 clone_instruction 深拷贝的指针。
    struct Entry {
        std::string    key;
        IRInstruction* value = nullptr;
    };
    using ListIter = std::list<Entry>::iterator;

    std::list<Entry>                              list_;
    std::unordered_map<std::string, ListIter>     map_;
    std::mutex                                    mtx_;
    static constexpr size_t                       kSoftLimit = 128;

    /** 拼接 key = "path_key:instr_name"。 */
    std::string make_key(const char* path_key, const char* instr_name) const;

    /** 淘汰 list_ 尾部（最近最少使用）元素。 */
    void evict_one_locked();

    /** 深拷贝 IRInstruction（含 metadata 链）。返回的指针需 bs_ir_instruction_destroy。 */
    static IRInstruction* clone_instruction(const IRInstruction* src);
};

} // namespace bs::app

#endif // BS_APP_SDK_CONFIG_SESSION_READER_H
