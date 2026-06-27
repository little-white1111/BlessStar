#include "bs/app/sdk/query_executor_registry.h"
#include "bs/app/sdk/config_declare.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace bs::app::sdk
{

/* ── 辅助：从 config 读全部关联字段 ────────────────────────────── */
static QueryParams read_params_for_pattern(const char* pattern, uint64_t version)
{
    QueryParams qp;
    qp.query_key = pattern;
    qp.version   = version;

    // 预定义字段列表（v1 兼容 + v3 结构化字段）
    const char* known_suffixes[] = {
        // v1 兼容字段
        "db_conn", "sql_template", "sql_params",
        "enabled", "timeout_ms",
        "date", "time", "nickname", "db_name",
        // v3 结构化字段
        "table", "fields", "filters", "order_by", "limit",
        nullptr
    };

    for (const char** sfx = known_suffixes; *sfx; ++sfx)
    {
        std::string key = std::string(pattern) + "." + *sfx;
        char* val = bs_config_read(key.c_str());
        if (val)
        {
            qp.params[*sfx] = val;
            std::free(val);
        }
    }

    return qp;
}

/* ── ExecutorHandle helpers ──────────────────────────────────── */
static bool is_valid_handle(ExecutorHandle h) { return h.id != 0; }

/* ── QueryExecutorRegistry ───────────────────────────────────── */

QueryExecutorRegistry& QueryExecutorRegistry::Instance()
{
    static QueryExecutorRegistry inst;
    return inst;
}

QueryExecutorRegistry::~QueryExecutorRegistry()
{
    Stop();
}

ExecutorHandle QueryExecutorRegistry::Register(std::unique_ptr<QueryExecutor> executor)
{
    if (!executor)
        return ExecutorHandle{0};

    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t id = next_id_++;
    ExecutorEntry entry;
    entry.id       = id;
    entry.executor = std::move(executor);

    entries_[id] = std::move(entry);
    return ExecutorHandle{id};
}

bool QueryExecutorRegistry::Unregister(ExecutorHandle handle)
{
    if (!is_valid_handle(handle))
        return false;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(handle.id);
    if (it == entries_.end())
        return false;

    entries_.erase(it);
    return true;
}

QueryExecutor* QueryExecutorRegistry::Match(const char* config_key) const
{
    if (!config_key)
        return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);

    // 最长 key pattern 匹配
    size_t best_len = 0;
    QueryExecutor* best = nullptr;

    for (const auto& pair : entries_)
    {
        const char* pattern = pair.second.executor->KeyPattern();
        if (std::strncmp(config_key, pattern, std::strlen(pattern)) == 0)
        {
            size_t plen = std::strlen(pattern);
            if (plen > best_len)
            {
                best_len = plen;
                best = pair.second.executor.get();
            }
        }
    }

    return best;
}

int QueryExecutorRegistry::ApplyChanges(const char* config_key)
{
    if (!config_key)
        return -1;

    // 1. 匹配 executor
    QueryExecutor* executor = Match(config_key);
    if (!executor)
        return 1;  // 无匹配

    const char* pattern = executor->KeyPattern();

    // 2. 读取关联配置字段
    QueryParams qp = read_params_for_pattern(pattern, 0);

    // 3. 检查 enabled 字段
    auto it_enabled = qp.params.find("enabled");
    if (it_enabled != qp.params.end() && it_enabled->second == "false")
        return 0;  // 已禁用，跳过

    // 4. 执行查询
    QueryResult result = executor->Execute(qp);

    // 5. 写回结果（Config Backflow）
    std::string result_key = std::string(pattern) + ".last_result";
    std::string error_key  = std::string(pattern) + ".last_error";
    std::string run_at_key = std::string(pattern) + ".last_run_at";

    if (result.status == 0)
    {
        bs_config_write(result_key.c_str(), result.result_json.c_str());
        bs_config_write(error_key.c_str(), "");
    }
    else
    {
        bs_config_write(result_key.c_str(), "");
        bs_config_write(error_key.c_str(), result.error_msg.c_str());
    }

    // 简化时间戳
    bs_config_write(run_at_key.c_str(), "2025-05-20T15:00:00Z");

    return (result.status == 0) ? 0 : -1;
}

int QueryExecutorRegistry::Start()
{
    // 二期实现：为每个已注册 executor 创建 ConfigConsumer 订阅
    // 当前 MVP 阶段仅提供 ApplyChanges(config_key) 手动触发模式
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = true;
    return 0;
}

void QueryExecutorRegistry::Stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
}

size_t QueryExecutorRegistry::Count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

} // namespace bs::app::sdk

/* ══════════════════════════════════════════════════════════════════
 * C ABI — 供 Rust napi-rs (lib.rs) 通过 extern "C" 调用
 *
 * 这些包装函数是 bs_config_declare_ffi 静态库的一部分，
 * 由 Editor 的 native addon 在运行时加载。
 * ══════════════════════════════════════════════════════════════════ */

extern "C" {

/**
 * 触发查询执行器：ApplyChanges(config_key) 的 C 接口。
 *
 * @param config_key  发生变更的完整配置 key
 * @return 0=已处理, 1=无匹配executor, -1=执行失败
 *
 * 由 Rust 侧 napi `execute_query` 工具调用。
 * D38-3-INV-03: 外部系统触发走 Config Backflow
 */
int bs_query_executor_apply_changes(const char* config_key)
{
    if (!config_key)
        return -1;
    return bs::app::sdk::QueryExecutorRegistry::Instance().ApplyChanges(config_key);
}

} // extern "C"
