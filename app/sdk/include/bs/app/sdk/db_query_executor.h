#ifndef BS_APP_SDK_DB_QUERY_EXECUTOR_H
#define BS_APP_SDK_DB_QUERY_EXECUTOR_H

/*
 * DbQueryExecutor — 数据库查询执行器
 *
 * 专题二（第38天）：动态配置热更扩展
 * D38-2-INV-03: 结果回写 config 是唯一通知路径
 * D38-2-INV-04: 复用现有 DB 层（bs_db_core / bs_db_mgmt）
 * D38-2-INV-05: 维持单向依赖
 */

#include "bs/app/sdk/query_executor.h"

#include <string>

namespace bs::app::sdk
{

/**
 * DbQueryExecutor — 数据库查询执行器。
 *
 * 从 config 读取查询参数，通过 DbMgr 连接池执行参数化 SQL，
 * 结果序列化为 JSON 后通过 bs_config_write 写回 config。
 */
class DbQueryExecutor : public QueryExecutor
{
public:
    /**
     * @param key_pattern  配置前缀，如 "query.audience"
     */
    explicit DbQueryExecutor(const char* key_pattern);

    ~DbQueryExecutor() override;

    // QueryExecutor 接口
    QueryResult  Execute(const QueryParams& params) override;
    const char*  KeyPattern() const override;
    const char*  ExecutorType() const override { return "db_query"; }

private:
    std::string key_pattern_;
};

} // namespace bs::app::sdk

#endif // BS_APP_SDK_DB_QUERY_EXECUTOR_H
