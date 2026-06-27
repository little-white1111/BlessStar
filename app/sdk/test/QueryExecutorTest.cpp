/*
 * QueryExecutorTest — 全链路测试
 *
 * 专题二（第38天）：动态配置热更扩展
 * 测试 DbQueryExecutor + QueryExecutorRegistry + DbConnector::ExecuteQuery
 *
 * 测试策略：
 * - 使用 SQLite 内存数据库（无需外部服务）
 * - 先创建表并插入数据，再通过 DbQueryExecutor 执行查询
 * - 验证结果 JSON 格式正确
 * - 验证 Config Backflow 模式正确写回 config
 */

#include "bs/app/sdk/query_executor.h"
#include "bs/app/sdk/query_executor_registry.h"
#include "bs/app/sdk/db_query_executor.h"
#include "bs/app/sdk/config_declare.h"
#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"
#include "bs/db/mgmt/db_mgr.h"
#include "bs/db/mgmt/db_mgr_config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cassert>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ── 测试辅助：生成唯一临时文件路径 ─────────────────────────────── */
static std::string g_temp_db_path;  // 全局 temp 路径，各测试复用

static std::string make_temp_db_path()
{
    char buf[256];
#ifdef _WIN32
    const char* tmpdir = getenv("TEMP");
    if (!tmpdir) tmpdir = ".";
    _snprintf(buf, sizeof(buf), "%s\\bs_test_qe_%d.db", tmpdir, _getpid());
#else
    const char* tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    snprintf(buf, sizeof(buf), "%s/bs_test_qe_%d.db", tmpdir, getpid());
#endif
    return std::string(buf);
}

/* ── 测试辅助：删除临时文件 ──────────────────────────────────────── */
static void cleanup_db_file(const std::string& path)
{
    if (!path.empty())
        remove(path.c_str());
}

/* ── 测试辅助：确保 SQLite 驱动已注册 ──────────────────────────── */
// sqlite_connector.cpp 中的静态初始化在静态库链接时可能被剥离，
// 因此这里手动注册。使用 forward declaration 代替 sqlite3.h 包含，
// 因为测试目标未配置 sqlite3 include 路径。
struct sqlite3;
struct sqlite3_stmt;
#define SQLITE_OK   0
#define SQLITE_ROW 100
#define SQLITE_DONE 101

typedef int (*sqlite3_callback)(void*, int, char**, char**);
extern "C" {
    int  sqlite3_open(const char* filename, sqlite3** ppDb);
    int  sqlite3_close(sqlite3* db);
    int  sqlite3_exec(sqlite3* db, const char* sql, sqlite3_callback cb, void* arg, char** errmsg);
    int  sqlite3_prepare_v2(sqlite3* db, const char* zSql, int nByte, sqlite3_stmt** ppStmt, const char** pzTail);
    int  sqlite3_step(sqlite3_stmt* pStmt);
    int  sqlite3_finalize(sqlite3_stmt* pStmt);
    int  sqlite3_bind_text(sqlite3_stmt* pStmt, int n, const char* zData, int nData, void (*destroy)(void*));
    const void* sqlite3_column_blob(sqlite3_stmt* pStmt, int iCol);
    int  sqlite3_column_bytes(sqlite3_stmt* pStmt, int iCol);
    const char* sqlite3_errmsg(sqlite3* db);
    void sqlite3_free(void* p);
}

class TestSqliteConnector : public bs::db::DbConnector
{
    sqlite3* db_ = nullptr;
public:
    explicit TestSqliteConnector(const bs::db::DatabaseConfig& cfg)
        : bs::db::DbConnector(cfg)
    {
        sqlite3_open(cfg.dsn.c_str(), &db_);
    }
    ~TestSqliteConnector() override { if (db_) sqlite3_close(db_); }

    bool FetchConfig(const char* tenant, const char* config_id,
                     std::vector<std::uint8_t>* out_data,
                     std::string* out_error) override
    {
        (void)tenant; (void)config_id; (void)out_data;
        if (out_error) *out_error = "FetchConfig not supported by test connector";
        return false;
    }

    bool FetchConfigs(const std::vector<std::pair<std::string, std::string>>& tenant_id_pairs,
                      std::vector<std::vector<std::uint8_t>>* out_results,
                      std::string* out_error) override
    {
        (void)tenant_id_pairs; (void)out_results;
        if (out_error) *out_error = "FetchConfigs not supported by test connector";
        return false;
    }

    const char* DriverName() const override { return "test_sqlite"; }

    // ── ExecuteQuery via sqlite3_exec ───────────────────────────
    bool ExecuteQuery(const char* sql,
                      const std::vector<std::string>& params,
                      std::vector<std::vector<std::string>>* out_rows,
                      std::vector<std::string>* out_cols,
                      std::string* out_error) override
    {
        if (!db_)
        {
            if (out_error) *out_error = "test_sqlite: not connected";
            return false;
        }
        // Build SQL by replacing '?' with escaped params
        std::string expanded_sql;
        size_t pi = 0;
        for (const char* p = sql; *p; ++p)
        {
            if (*p == '?' && pi < params.size())
            {
                expanded_sql += '\'';
                for (char c : params[pi++])
                {
                    if (c == '\'') expanded_sql += '\'\'';
                    else expanded_sql += c;
                }
                expanded_sql += '\'';
            }
            else
                expanded_sql += *p;
        }

        struct Ctx { std::vector<std::vector<std::string>>* r; std::vector<std::string>* c; bool first = true; };
        Ctx ctx; ctx.r = out_rows; ctx.c = out_cols;
        if (out_rows) out_rows->clear();

        auto cb = [](void* a, int argc, char** argv, char** colv) -> int
        {
            auto* p = static_cast<Ctx*>(a);
            if (p->first && p->c)
            {
                p->c->clear();
                for (int i = 0; i < argc; ++i) p->c->push_back(colv[i] ? colv[i] : "");
                p->first = false;
            }
            if (p->r)
            {
                std::vector<std::string> row;
                for (int i = 0; i < argc; ++i) row.push_back(argv[i] ? argv[i] : "");
                p->r->push_back(std::move(row));
            }
            return 0;
        };

        char* err = nullptr;
        int rc = sqlite3_exec(db_, expanded_sql.c_str(), cb, &ctx, &err);
        if (rc != SQLITE_OK)
        {
            if (out_error) *out_error = err ? err : "sqlite3_exec failed";
            if (err) sqlite3_free(err);
            return false;
        }
        if (err) sqlite3_free(err);
        return true;
    }
};

static void ensure_sqlite_driver()
{
    if (bs::db::DbDriverFactory::Instance().Has(bs::db::DbDriverType::SQLite))
        return;

    bs::db::DbDriverFactory::Instance().Register(bs::db::DbDriverType::SQLite,
        [](const bs::db::DatabaseConfig& cfg) -> bs::db::DbConnector* {
            return new TestSqliteConnector(cfg);
        });
}

/* ── 测试辅助：清理 config 状态 ────────────────────────────────── */
static void reset_config()
{
    bs_config_declare_reset();

    // 声明测试用的配置字段 — 直接数组声明避免宏展开问题
    const bs_field_decl_t test_fields[] = {
        { "query.test.db_conn",      BS_TYPE_STRING, "sqlite:test.db",            "数据库连接", false },
        { "query.test.sql_template",  BS_TYPE_STRING,
          "SELECT * FROM messages WHERE date=? AND time=? AND nickname=?",
          "SQL模板", false },
        { "query.test.sql_params",   BS_TYPE_STRING, "[\"2025-05-20\",\"15:00\",\"小白\"]", "SQL参数", false },
        { "query.test.enabled",      BS_TYPE_BOOL,   "true",                       "是否启用", false },
        { "query.test.last_result",  BS_TYPE_STRING, "",                           "上次查询结果（只读）", false },
        { "query.test.last_error",   BS_TYPE_STRING, "",                           "上次错误信息（只读）", false },
        { "query.test.last_run_at",  BS_TYPE_STRING, "",                           "上次执行时间（只读）", false },
    };
    bs_config_declare(test_fields, sizeof(test_fields) / sizeof(test_fields[0]));

    // 写入测试参数（用 SQLite 文件库）
    g_temp_db_path = make_temp_db_path();
    bs_config_write("query.test.db_conn", g_temp_db_path.c_str());
    bs_config_write("query.test.sql_template",
                    "SELECT msg, nickname, date, time FROM messages WHERE date=? AND time=? AND nickname=?");
    bs_config_write("query.test.sql_params", "[\"2025-05-20\",\"15:00\",\"小白\"]");
    bs_config_write("query.test.enabled", "true");
}

/* ── 测试辅助：创建测试数据表并插入数据 ─────────────────────────── */
static bool setup_test_data(const std::string& db_path, std::string* error)
{
    // 确保 SQLite 驱动已注册
    ensure_sqlite_driver();

    // 先删除可能残留的旧文件
    cleanup_db_file(db_path);

    // 手动创建 SQLite 文件库并插入测试数据
    bs::db::mgmt::DbMgrConfig mgr_cfg;
    mgr_cfg.db_cfg.driver_type = bs::db::DbDriverType::SQLite;
    mgr_cfg.db_cfg.dsn = db_path;
    mgr_cfg.pool_size = 1;

    if (!bs::db::mgmt::DbMgr::Instance().Open(mgr_cfg))
    {
        if (error) *error = "DbMgr::Open failed";
        printf("  setup_test_data: DbMgr::Open failed (driver=%d, dsn=%s, pool=%d)\n",
               (int)mgr_cfg.db_cfg.driver_type, mgr_cfg.db_cfg.dsn.c_str(), mgr_cfg.pool_size);
        return false;
    }

    auto* conn = bs::db::mgmt::DbMgr::Instance().Acquire();
    if (!conn)
    {
        if (error) *error = "Acquire failed";
        return false;
    }

    // 创建表 — 用 ExecuteQuery
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> cols;
    std::string err;
    bool ok = conn->ExecuteQuery(
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date TEXT NOT NULL,"
        "time TEXT NOT NULL,"
        "nickname TEXT NOT NULL,"
        "msg TEXT NOT NULL)",
        {}, &rows, &cols, &err);

    if (!ok)
    {
        if (error) *error = "CREATE TABLE failed: " + err;
        bs::db::mgmt::DbMgr::Instance().Release(conn);
        return false;
    }

    // 插入测试数据
    ok = conn->ExecuteQuery(
        "INSERT INTO messages (date, time, nickname, msg) VALUES (?, ?, ?, ?)",
        {"2025-05-20", "15:00", "小白", "你好，大家好"},
        nullptr, nullptr, &err);
    if (!ok)
    {
        if (error) *error = "INSERT 1 failed: " + err;
        bs::db::mgmt::DbMgr::Instance().Release(conn);
        return false;
    }

    ok = conn->ExecuteQuery(
        "INSERT INTO messages (date, time, nickname, msg) VALUES (?, ?, ?, ?)",
        {"2025-05-20", "15:00", "小白", "今天天气真好"},
        nullptr, nullptr, &err);
    if (!ok)
    {
        if (error) *error = "INSERT 2 failed: " + err;
        bs::db::mgmt::DbMgr::Instance().Release(conn);
        return false;
    }

    ok = conn->ExecuteQuery(
        "INSERT INTO messages (date, time, nickname, msg) VALUES (?, ?, ?, ?)",
        {"2025-05-21", "10:00", "小红", "不会被查到"},
        nullptr, nullptr, &err);
    if (!ok)
    {
        if (error) *error = "INSERT 3 failed: " + err;
        bs::db::mgmt::DbMgr::Instance().Release(conn);
        return false;
    }

    bs::db::mgmt::DbMgr::Instance().Release(conn);
    return true;
}

/* ── Test 1: QueryExecutor 接口基本功能 ────────────────────────── */
static int test_query_executor_basic()
{
    printf("[Test] query_executor_basic ... ");

    reset_config();

    // 注册 DbQueryExecutor
    using namespace bs::app::sdk;
    auto executor = std::make_unique<DbQueryExecutor>("query.test");
    const char* pattern = executor->KeyPattern();
    assert(std::strcmp(pattern, "query.test") == 0);
    assert(std::strcmp(executor->ExecutorType(), "db_query") == 0);

    printf("PASS\n");
    return 0;
}

/* ── Test 2: QueryExecutorRegistry 注册/卸载/匹配 ─────────────── */
static int test_registry_register_unregister()
{
    printf("[Test] registry_register_unregister ... ");

    using namespace bs::app::sdk;
    auto& reg = QueryExecutorRegistry::Instance();

    auto exec1 = std::make_unique<DbQueryExecutor>("query.test");
    auto exec2 = std::make_unique<DbQueryExecutor>("query.other");

    auto h1 = reg.Register(std::move(exec1));
    auto h2 = reg.Register(std::move(exec2));

    assert(reg.Count() == 2);

    // 最长匹配
    auto* matched = reg.Match("query.test.date");
    assert(matched != nullptr);
    assert(std::strcmp(matched->KeyPattern(), "query.test") == 0);

    matched = reg.Match("query.other.x");
    assert(matched != nullptr);
    assert(std::strcmp(matched->KeyPattern(), "query.other") == 0);

    // 卸载
    assert(reg.Unregister(h1));
    assert(reg.Count() == 1);

    // 已卸载的应匹配不到
    matched = reg.Match("query.test.date");
    assert(matched == nullptr);

    // 无效 handle 卸载应失败
    assert(!reg.Unregister(ExecutorHandle{999}));

    // 清理剩下的
    reg.Unregister(h2);
    assert(reg.Count() == 0);

    printf("PASS\n");
    return 0;
}

/* ── Test 3: DbQueryExecutor 全链路 ────────────────────────────── */
static int test_db_query_executor_full_chain()
{
    printf("[Test] db_query_executor_full_chain ... ");

    reset_config();

    // 设置测试数据
    std::string setup_err;
    bool setup_ok = setup_test_data(g_temp_db_path, &setup_err);
    assert(setup_ok && "setup_test_data failed");

    // 读取参数
    using namespace bs::app::sdk;
    QueryParams params;
    params.query_key = "query.test";
    params.version = 1;

    char* conn = bs_config_read("query.test.db_conn");
    char* sql  = bs_config_read("query.test.sql_template");
    char* sparams = bs_config_read("query.test.sql_params");

    assert(conn != nullptr);
    assert(sql != nullptr);
    assert(sparams != nullptr);

    params.params["db_conn"]       = conn ? conn : "";
    params.params["sql_template"]  = sql ? sql : "";
    params.params["sql_params"]    = sparams ? sparams : "[]";

    std::free(conn);
    std::free(sql);
    std::free(sparams);

    // 执行查询
    DbQueryExecutor executor("query.test");
    QueryResult result = executor.Execute(params);

    assert(result.status == 0);
    assert(!result.result_json.empty());
    assert(result.error_msg.empty());

    // 验证 JSON 结果
    printf("  Result JSON: %s\n", result.result_json.c_str());

    // 应包含 2 行结果（"你好，大家好" 和 "今天天气真好"）
    assert(result.result_json.find("\"row_count\":2") != std::string::npos);
    assert(result.result_json.find("你好") != std::string::npos);
    assert(result.result_json.find("今天天气真好") != std::string::npos);
    // 不应包含 "不会被查到"（这条数据是 5月21日 小红的）
    assert(result.result_json.find("不会被查到") == std::string::npos);

    // 验证 Config Backflow — 结果已写回 config
    char* last_result = bs_config_read("query.test.last_result");
    assert(last_result != nullptr);
    assert(std::strlen(last_result) > 0);
    printf("  last_result written: %s\n", last_result);
    std::free(last_result);

    // 清理
    bs::db::mgmt::DbMgr::Instance().Close();

    printf("PASS\n");
    return 0;
}

/* ── Test 4: 空结果查询 ────────────────────────────────────────── */
static int test_db_query_empty_result()
{
    printf("[Test] db_query_empty_result ... ");

    reset_config();

    std::string setup_err;
    bool setup_ok = setup_test_data(g_temp_db_path, &setup_err);
    assert(setup_ok);

    // 查询不存在的数据
    bs_config_write("query.test.sql_params", "[\"2099-01-01\",\"00:00\",\"不存在\"]");

    using namespace bs::app::sdk;
    QueryParams params;
    params.query_key = "query.test";
    params.version = 2;

    char* conn = bs_config_read("query.test.db_conn");
    char* sql  = bs_config_read("query.test.sql_template");
    char* sparams = bs_config_read("query.test.sql_params");

    params.params["db_conn"]      = conn ? conn : "";
    params.params["sql_template"] = sql ? sql : "";
    params.params["sql_params"]   = sparams ? sparams : "[]";

    std::free(conn);
    std::free(sql);
    std::free(sparams);

    DbQueryExecutor executor("query.test");
    QueryResult result = executor.Execute(params);

    assert(result.status == 0);
    assert(result.result_json.find("\"row_count\":0") != std::string::npos);

    printf("  Empty result: %s\n", result.result_json.c_str());

    bs::db::mgmt::DbMgr::Instance().Close();
    printf("PASS\n");
    return 0;
}

/* ── Test 5: 错误处理 — SQL 语法错误 ───────────────────────────── */
static int test_db_query_sql_error()
{
    printf("[Test] db_query_sql_error ... ");

    reset_config();

    std::string setup_err;
    setup_test_data(g_temp_db_path, &setup_err);

    using namespace bs::app::sdk;
    QueryParams params;
    params.query_key = "query.test";
    params.version = 3;
    params.params["db_conn"]      = g_temp_db_path;
    params.params["sql_template"] = "SELECT * FROM nonexistent_table WHERE x=?";
    params.params["sql_params"]   = "[\"test\"]";

    DbQueryExecutor executor("query.test");
    QueryResult result = executor.Execute(params);

    // SQLite will return an error for nonexistent table
    assert(result.status == -1 || !result.error_msg.empty());

    printf("  Error result: status=%d, error=%s\n",
           result.status, result.error_msg.c_str());

    // Error should be written back to config
    char* last_error = bs_config_read("query.test.last_error");
    if (last_error)
    {
        printf("  last_error written: %s\n", last_error);
        std::free(last_error);
    }

    bs::db::mgmt::DbMgr::Instance().Close();
    printf("PASS\n");
    return 0;
}

/* ── Test 6: 多字段 SQL 参数 ───────────────────────────────────── */
static int test_db_query_multi_params()
{
    printf("[Test] db_query_multi_params ... ");

    reset_config();

    std::string setup_err;
    setup_test_data(g_temp_db_path, &setup_err);

    // 用不同参数查询
    bs_config_write("query.test.sql_params", "[\"2025-05-21\",\"10:00\",\"小红\"]");

    using namespace bs::app::sdk;
    QueryParams params;
    params.query_key = "query.test";
    params.version = 4;
    params.params["db_conn"]      = g_temp_db_path;
    params.params["sql_template"] = "SELECT msg, nickname FROM messages WHERE date=? AND time=? AND nickname=?";
    params.params["sql_params"]   = "[\"2025-05-21\",\"10:00\",\"小红\"]";

    DbQueryExecutor executor("query.test");
    QueryResult result = executor.Execute(params);

    assert(result.status == 0);
    assert(result.result_json.find("\"row_count\":1") != std::string::npos);
    assert(result.result_json.find("不会被查到") != std::string::npos);

    printf("  Result: %s\n", result.result_json.c_str());

    bs::db::mgmt::DbMgr::Instance().Close();
    printf("PASS\n");
    return 0;
}

/* ── Test 7: v3 结构化参数路径 ───────────────────────────────── */
static int test_db_query_structured_params()
{
    printf("[Test] db_query_structured_params ... ");

    reset_config();

    std::string setup_err;
    setup_test_data(g_temp_db_path, &setup_err);

    using namespace bs::app::sdk;
    QueryParams params;
    params.query_key = "query.test";
    params.version = 5;

    // 使用 v3 结构化参数（不提供 sql_template）
    params.params["db_conn"]   = g_temp_db_path;
    params.params["table"]     = "messages";
    params.params["fields"]    = "[\"msg\",\"nickname\",\"date\"]";
    params.params["filters"]   = "{\"date\":\"2025-05-20\",\"nickname\":\"小白\"}";
    params.params["order_by"]  = "date DESC";
    params.params["limit"]     = "50";

    DbQueryExecutor executor("query.test");
    QueryResult result = executor.Execute(params);

    assert(result.status == 0);
    assert(!result.result_json.empty());
    assert(result.error_msg.empty());

    printf("  Result JSON: %s\n", result.result_json.c_str());

    // 应包含 2 行结果（"你好，大家好" 和 "今天天气真好"）
    assert(result.result_json.find("\"row_count\":2") != std::string::npos);
    assert(result.result_json.find("你好") != std::string::npos);
    assert(result.result_json.find("今天天气真好") != std::string::npos);
    // 不应包含 "不会被查到"（小红的数据，date=2025-05-21）
    assert(result.result_json.find("不会被查到") == std::string::npos);

    // 验证列名正确（fields 指定的 msg/nickname/date）
    assert(result.result_json.find("msg") != std::string::npos);
    assert(result.result_json.find("nickname") != std::string::npos);
    assert(result.result_json.find("date") != std::string::npos);

    // 验证 Config Backflow
    char* last_result = bs_config_read("query.test.last_result");
    assert(last_result != nullptr);
    assert(std::strlen(last_result) > 0);
    printf("  last_result (structured): %s\n", last_result);
    std::free(last_result);

    // ── 测试：空 fields（应 SELECT *） ──────────────────────────
    QueryParams params2;
    params2.query_key = "query.test";
    params2.version = 6;
    params2.params["db_conn"]  = g_temp_db_path;
    params2.params["table"]    = "messages";
    params2.params["filters"]  = "{\"date\":\"2025-05-20\"}";

    QueryResult result2 = executor.Execute(params2);
    assert(result2.status == 0);
    // SELECT * 应包含所有列
    assert(result2.result_json.find("nickname") != std::string::npos);
    assert(result2.result_json.find("msg") != std::string::npos);
    printf("  SELECT * result: %s\n", result2.result_json.c_str());

    bs::db::mgmt::DbMgr::Instance().Close();
    printf("PASS\n");
    return 0;
}

/* ── main ──────────────────────────────────────────────────────── */
int main()
{
    int failed = 0;

    failed += test_query_executor_basic();
    failed += test_registry_register_unregister();
    failed += test_db_query_executor_full_chain();
    failed += test_db_query_empty_result();
    failed += test_db_query_sql_error();
    failed += test_db_query_multi_params();
    failed += test_db_query_structured_params();

    printf("\n%d tests passed, %d failed\n", 7 - failed, failed);
    return failed > 0 ? 1 : 0;
}
