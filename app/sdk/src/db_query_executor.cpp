#include "bs/app/sdk/db_query_executor.h"
#include "bs/app/sdk/config_declare.h"   // bs_config_read / bs_config_write
#include "bs/db/mgmt/db_mgr.h"
#include "bs/db/mgmt/db_mgr_config.h"

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace bs::app::sdk
{

/* ── 结构化参数 → SQL 构建 ────────────────────────────────────── *
 * v3 演进：替代 sql_template/sql_params，从 table/filters/fields
 * 等语义字段构建参数化 SQL。
 *
 * D38-3-INV-07: 查询参数结构化，不存原生 SQL
 * ────────────────────────────────────────────────────────────────── */

/**
 * 辅助：简单 JSON 数组解析 → vector<string>
 * 如 "[\"msg\",\"nickname\"]" → {"msg", "nickname"}
 */
static std::vector<std::string> parse_json_array(const std::string& arr)
{
    std::vector<std::string> result;
    size_t start = arr.find('[');
    size_t end   = arr.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end <= start)
        return result;

    std::string inner = arr.substr(start + 1, end - start - 1);
    size_t pos = 0;
    while (pos < inner.size())
    {
        // 跳过空白、逗号、引号
        while (pos < inner.size() && (inner[pos] == ' ' || inner[pos] == ',' || inner[pos] == '"' || inner[pos] == '\n' || inner[pos] == '\r' || inner[pos] == '\t'))
            ++pos;
        size_t val_start = pos;
        while (pos < inner.size() && inner[pos] != ',' && inner[pos] != ']')
            ++pos;
        if (pos > val_start)
        {
            std::string val = inner.substr(val_start, pos - val_start);
            // 去掉尾部空白和引号
            while (!val.empty() && (val.back() == '"' || val.back() == ' ' || val.back() == '\t'))
                val.pop_back();
            if (!val.empty())
                result.push_back(val);
        }
    }
    return result;
}

/**
 * 辅助：简单 JSON 对象解析 → key-value pairs
 * 如 "{\"date\":\"2025-05-20\",\"nickname\":\"小白\"}"
 * → {{"date","2025-05-20"}, {"nickname","小白"}}
 *
 * 仅提取第一层 string:string 对，不处理嵌套。
 */
static std::vector<std::pair<std::string, std::string>> parse_json_object(const std::string& obj)
{
    std::vector<std::pair<std::string, std::string>> result;
    size_t start = obj.find('{');
    size_t end   = obj.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end <= start)
        return result;

    std::string inner = obj.substr(start + 1, end - start - 1);
    // 按逗号分割 key:value 对
    size_t pos = 0;
    while (pos < inner.size())
    {
        // 跳过空白和逗号
        while (pos < inner.size() && (inner[pos] == ' ' || inner[pos] == ',' || inner[pos] == '\n' || inner[pos] == '\r' || inner[pos] == '\t'))
            ++pos;
        if (pos >= inner.size())
            break;

        // 找 key（引号间）
        if (inner[pos] != '"')
            break;
        ++pos; // 跳过起始引号
        size_t key_start = pos;
        while (pos < inner.size() && inner[pos] != '"')
            ++pos;
        if (pos >= inner.size())
            break;
        std::string key = inner.substr(key_start, pos - key_start);
        ++pos; // 跳过结束引号

        // 跳过冒号和空白
        while (pos < inner.size() && (inner[pos] == ' ' || inner[pos] == ':' || inner[pos] == '\t'))
            ++pos;

        // 找 value（引号间）
        if (pos < inner.size() && inner[pos] == '"')
        {
            ++pos;
            size_t val_start = pos;
            while (pos < inner.size() && inner[pos] != '"')
            {
                // 处理转义引号
                if (inner[pos] == '\\' && pos + 1 < inner.size())
                    ++pos;
                ++pos;
            }
            std::string val = inner.substr(val_start, pos - val_start);
            // 去除尾部空白
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
                val.pop_back();
            result.push_back({key, val});
            ++pos; // 跳过结束引号
        }
    }
    return result;
}

/**
 * 从结构化字段构建参数化 SQL 和绑定参数。
 *
 * @param table      表名
 * @param fields_json  字段名 JSON 数组，如 ["msg","nickname"]
 * @param filters_json 过滤条件 JSON 对象，如 {"date":"2025-05-20"}
 * @param order_by     排序子句（可选）
 * @param limit        最大行数（0 表示不限制）
 * @param[out] out_params  输出：绑定的参数值列表（按 filters 中 ? 的顺序）
 * @return 构造的 SQL 字符串
 */
static std::string build_structured_sql(
    const std::string& table,
    const std::string& fields_json,
    const std::string& filters_json,
    const std::string& order_by,
    int limit,
    std::vector<std::string>& out_params)
{
    std::ostringstream sql;

    // SELECT fields
    sql << "SELECT ";
    auto fields = parse_json_array(fields_json);
    if (fields.empty())
    {
        sql << "*";
    }
    else
    {
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0) sql << ", ";
            sql << fields[i];
        }
    }

    // FROM table
    sql << " FROM " << table;

    // WHERE filters
    auto filters = parse_json_object(filters_json);
    if (!filters.empty())
    {
        sql << " WHERE ";
        for (size_t i = 0; i < filters.size(); ++i)
        {
            if (i > 0) sql << " AND ";
            sql << filters[i].first << "=?";
            out_params.push_back(filters[i].second);
        }
    }

    // ORDER BY
    if (!order_by.empty())
    {
        sql << " ORDER BY " << order_by;
    }

    // LIMIT
    if (limit > 0)
    {
        sql << " LIMIT " << limit;
    }

    return sql.str();
}

/* ── DbQueryExecutor 实现 ─────────────────────────────────────── */

DbQueryExecutor::DbQueryExecutor(const char* key_pattern)
    : key_pattern_(key_pattern ? key_pattern : "")
{
}

DbQueryExecutor::~DbQueryExecutor() = default;

const char* DbQueryExecutor::KeyPattern() const
{
    return key_pattern_.c_str();
}

QueryResult DbQueryExecutor::Execute(const QueryParams& params)
{
    QueryResult result;
    result.version = params.version;

    // 1. 提取必需的查询参数
    auto it_conn   = params.params.find("db_conn");

    if (it_conn == params.params.end() || it_conn->second.empty())
    {
        result.error_msg = params.query_key + ": missing/empty db_conn";
        return result;
    }

    // 检查 enabled 字段
    auto it_enabled = params.params.find("enabled");
    if (it_enabled != params.params.end() && it_enabled->second == "false")
    {
        result.status = 0;
        result.result_json = "{\"row_count\":0,\"columns\":[],\"rows\":[],\"disabled\":true}";
        return result;
    }

    // 2. 确定查询模式：v3 结构化（table 存在）或 v1 兼容（sql_template 存在）
    auto it_table = params.params.find("table");
    auto it_sql   = params.params.find("sql_template");

    std::string sql;
    std::vector<std::string> bind_params;

    if (it_table != params.params.end() && !it_table->second.empty())
    {
        // ── v3 结构化参数模式 ──
        auto it_fields   = params.params.find("fields");
        auto it_filters  = params.params.find("filters");
        auto it_order_by = params.params.find("order_by");
        auto it_limit    = params.params.find("limit");

        int limit = 100; // 默认 100
        if (it_limit != params.params.end() && !it_limit->second.empty())
        {
            try { limit = std::stoi(it_limit->second); }
            catch (...) { limit = 100; }
            if (limit <= 0) limit = 100;
        }

        sql = build_structured_sql(
            it_table->second,
            it_fields != params.params.end() ? it_fields->second : "",
            it_filters != params.params.end() ? it_filters->second : "",
            it_order_by != params.params.end() ? it_order_by->second : "",
            limit,
            bind_params);
    }
    else if (it_sql != params.params.end() && !it_sql->second.empty())
    {
        // ── v1 兼容模式：sql_template ──
        sql = it_sql->second;

        // 解析 sql_params — JSON 数组
        auto it_params = params.params.find("sql_params");
        if (it_params != params.params.end() && !it_params->second.empty())
        {
            bind_params = parse_json_array(it_params->second);
        }
    }
    else
    {
        result.error_msg = params.query_key + ": missing both table (structured) and sql_template (legacy)";
        return result;
    }

    // 3. 配置数据库连接 — 使用 DbMgr 连接池
    bs::db::mgmt::DbMgrConfig mgr_cfg;
    bs::db::DatabaseConfig& db_cfg = mgr_cfg.db_cfg;
    db_cfg.dsn = it_conn->second;
    if (db_cfg.dsn.find("sqlite:") == 0 || db_cfg.dsn.find(".db") != std::string::npos)
        db_cfg.driver_type = bs::db::DbDriverType::SQLite;
    else
        db_cfg.driver_type = bs::db::DbDriverType::MySQL;
    mgr_cfg.pool_size = 1;

    bs::db::mgmt::DbMgr::Instance().Open(mgr_cfg);
    bs::db::DbConnector* conn = bs::db::mgmt::DbMgr::Instance().Acquire();
    if (!conn)
    {
        result.error_msg = params.query_key + ": failed to acquire DB connection";
        return result;
    }

    // 4. 执行参数化 SQL
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> cols;
    std::string db_error;
    bool ok = conn->ExecuteQuery(sql.c_str(), bind_params, &rows, &cols, &db_error);
    bs::db::mgmt::DbMgr::Instance().Release(conn);

    if (!ok)
    {
        result.error_msg = params.query_key + ": query failed - " + db_error;

        // 回写错误信息到 config（Config Backflow 模式）
        std::string error_key = key_pattern_ + ".last_error";
        std::string result_key = key_pattern_ + ".last_result";
        std::string run_at_key = key_pattern_ + ".last_run_at";
        bs_config_write(error_key.c_str(), result.error_msg.c_str());
        bs_config_write(result_key.c_str(), "");
        bs_config_write(run_at_key.c_str(), "2025-05-20T15:00:00Z");

        return result;
    }

    // 5. 序列化结果为 JSON
    std::ostringstream json;
    json << "{\"row_count\":" << rows.size();
    json << ",\"columns\":[";
    for (size_t i = 0; i < cols.size(); ++i)
    {
        if (i > 0) json << ",";
        json << "\"" << cols[i] << "\"";
    }
    json << "],\"rows\":[";
    for (size_t r = 0; r < rows.size(); ++r)
    {
        if (r > 0) json << ",";
        json << "[";
        for (size_t c = 0; c < rows[r].size(); ++c)
        {
            if (c > 0) json << ",";
            json << "\"" << rows[r][c] << "\"";
        }
        json << "]";
    }
    json << "]}";

    result.status = 0;
    result.result_json = json.str();

    // 6. 回写结果到 config（Config Backflow）
    std::string result_key  = key_pattern_ + ".last_result";
    std::string run_at_key  = key_pattern_ + ".last_run_at";
    std::string error_key   = key_pattern_ + ".last_error";

    bs_config_write(result_key.c_str(), result.result_json.c_str());
    bs_config_write(run_at_key.c_str(), "2025-05-20T15:00:00Z");
    bs_config_write(error_key.c_str(), "");

    return result;
}

} // namespace bs::app::sdk
