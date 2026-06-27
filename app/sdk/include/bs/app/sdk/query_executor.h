#ifndef BS_APP_SDK_QUERY_EXECUTOR_H
#define BS_APP_SDK_QUERY_EXECUTOR_H

/*
 * QueryExecutor — 运行时查询执行器抽象接口
 *
 * 专题二（第38天）：动态配置热更扩展
 * D38-2-INV-01: 不入侵 Adapter/Kernel 层，仅使用 App SDK 已有 API
 * D38-2-INV-05: 维持单向依赖
 */

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace bs::app::sdk
{

/**
 * 查询参数快照（从 config 系统读取的全部关联字段）。
 */
struct QueryParams
{
    std::string                                query_key;   // 配置前缀，如 "query.audience"
    std::unordered_map<std::string, std::string> params;    // 字段名 → 值
    uint64_t                                   version;     // 触发本次查询的配置版本号
};

/**
 * 查询结果。
 */
struct QueryResult
{
    int         status      = -1;   // 0=成功, 非0=错误码
    std::string result_json;        // 结果序列化为 JSON
    std::string error_msg;          // 错误信息
    uint64_t    version     = 0;    // 触发本次查询的配置版本号
};

/**
 * 查询执行器抽象接口。
 * 所有具体执行器（DB 查询、HTTP 调用等）实现此接口。
 */
class QueryExecutor
{
public:
    virtual ~QueryExecutor() = default;

    /** 执行查询。 */
    virtual QueryResult Execute(const QueryParams& params) = 0;

    /** 返回此执行器监听的配置 key 前缀。如 "query.audience"。 */
    virtual const char* KeyPattern() const = 0;

    /** 返回执行器类型名称（日志/诊断用）。如 "db_query"。 */
    virtual const char* ExecutorType() const = 0;
};

} // namespace bs::app::sdk

#endif // BS_APP_SDK_QUERY_EXECUTOR_H
