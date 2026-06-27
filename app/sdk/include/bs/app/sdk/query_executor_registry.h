#ifndef BS_APP_SDK_QUERY_EXECUTOR_REGISTRY_H
#define BS_APP_SDK_QUERY_EXECUTOR_REGISTRY_H

/*
 * QueryExecutorRegistry — 运行时业务行为注册表
 *
 * 专题二（第38天）：动态配置热更扩展
 * D38-2-INV-01: 不入侵 Adapter/Kernel 层，仅使用 App SDK 已有 API
 * D38-2-INV-02: 保持 Adapter 插件体系不变
 * D38-2-INV-03: 结果回写 config 是唯一通知路径
 * D38-2-INV-05: 维持单向依赖
 */

#include "bs/app/sdk/query_executor.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace bs::app::sdk
{

/** 注册句柄，用于卸载。 */
struct ExecutorHandle
{
    uint64_t id = 0;
};

/**
 * 运行时业务行为注册表。
 *
 * 业务系统通过 Register() 注册 QueryExecutor，
 * Register() 后调用 Start() 启动监听。
 *
 * 内部：为每个 executor 的 key pattern 建立 ConfigConsumer 订阅，
 * 变更时自动执行 Execute() 并将结果写回 config。
 */
class QueryExecutorRegistry
{
public:
    static QueryExecutorRegistry& Instance();

    /**
     * 注册一个查询执行器。
     * @param executor  执行器对象（Registry 取得所有权）
     * @return 句柄，用于后续 Unregister
     */
    ExecutorHandle Register(std::unique_ptr<QueryExecutor> executor);

    /**
     * 卸载执行器。
     * @return false 表示 handle 无效
     */
    bool Unregister(ExecutorHandle handle);

    /**
     * 按 config key 匹配已注册的执行器（最长 key pattern 匹配）。
     */
    QueryExecutor* Match(const char* config_key) const;

    /**
     * 响应配置变更：匹配 executor → 读取关联配置字段 → 执行 → 写回结果。
     *
     * 业务系统在每次 bs_config_write 后调用此方法：
     * @code
     *   bs_config_write("query.audience.date", "2025-05-20");
     *   QueryExecutorRegistry::Instance().ApplyChanges("query.audience.date");
     * @endcode
     *
     * ApplyChanges 内部流程：
     *   1. Match(config_key) 找到最长匹配的 executor
     *   2. 读取该 executor KeyPattern() 下的所有关联配置字段
     *   3. 检查 enabled 字段（为 false 跳过）
     *   4. 调用 executor->Execute(params)
     *   5. 结果写回 config（Config Backflow 模式）
     *
     * @param config_key  发生变更的完整配置 key
     * @return 0=已处理, 1=无匹配executor, -1=执行失败
     */
    int ApplyChanges(const char* config_key);

    /**
     * 启动所有执行器的监听（二期实现）。
     *
     * 当前 MVP 阶段仅提供 ApplyChanges(config_key) 手动触发模式。
     * 二期待 SHM diff 检测机制就绪后，此方法将自动为每个已注册的
     * executor 建立 ConfigConsumer 订阅。
     *
     * @return 0 成功（当前版本恒返回 0）
     */
    int Start();

    /** 停止所有监听。 */
    void Stop();

    /** 当前注册的执行器数量。 */
    size_t Count() const;

private:
    QueryExecutorRegistry() = default;
    ~QueryExecutorRegistry();
    QueryExecutorRegistry(const QueryExecutorRegistry&) = delete;
    QueryExecutorRegistry& operator=(const QueryExecutorRegistry&) = delete;

    struct ExecutorEntry
    {
        uint64_t                         id;
        std::unique_ptr<QueryExecutor>   executor;
        // 内部的 ConfigConsumer 对象，用 void* 避免包含 consumer 头
        void*                            consumer = nullptr;
    };

    mutable std::mutex mutex_;
    uint64_t next_id_ = 1;
    std::unordered_map<uint64_t, ExecutorEntry> entries_;
    bool started_ = false;
};

} // namespace bs::app::sdk

#endif // BS_APP_SDK_QUERY_EXECUTOR_REGISTRY_H
