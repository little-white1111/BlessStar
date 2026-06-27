#include "bs/app/sdk/app_session.h"
#include "bs/app/sdk/config_declare.h"

#include "bs/adapter/registry_bootstrap.h"
#include "bs/db/db_factory.h"
#include "bs/db/mgmt/db_mgr.h"
#include "bs/db/mgmt/db_mgr_config.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace bs::app
{

// helpers

static void bootstrap_ctx(AttachContext*& ctx, IoFacade*& io, bool& ok)
{
    ctx = bs_adapter_attach_ctx_create();
    if (!ctx)
        return;

    if (bs_adapter_registry_bootstrap_begin_ctx(ctx) != 0)
    {
        ok = false;
        return;
    }

    if (bs_adapter_registry_bootstrap_freeze_ctx(ctx) != 0)
    {
        ok = false;
        return;
    }

    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    if (facade)
    {
        io = bs_io_facade_create(facade);
    }
    ok = true;
}

/* ── Manifest 辅助：从 manifest.json 读取 data_file 路径 ──────────── */
static char* resolve_data_file_path(const char* manifest_path)
{
    if (!manifest_path || !manifest_path[0]) return nullptr;

    FILE* f = fopen(manifest_path, "r");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);
    if (fsize <= 0) { fclose(f); return nullptr; }

    char* content = (char*)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return nullptr; }

    size_t read_size = fread(content, 1, (size_t)fsize, f);
    content[read_size] = '\0';
    fclose(f);
    if (read_size == 0) { free(content); return nullptr; }

    /* 查找 "data_file" 字段的值 */
    const char* needle = "\"data_file\"";
    char* p = strstr(content, needle);
    if (!p) { free(content); return nullptr; }

    p += strlen(needle);
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != ':') { free(content); return nullptr; }
    p++;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != '"') { free(content); return nullptr; }
    p++;

    std::string data_file;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) p++;
        data_file += *p;
        p++;
    }
    free(content);

    if (data_file.empty()) return nullptr;

    /* 获取 manifest 所在目录，拼接 data_file 路径 */
    const char* last_sep = nullptr;
    for (const char* cp = manifest_path; *cp; cp++) {
        if (*cp == '/' || *cp == '\\') last_sep = cp;
    }

    std::string result;
    if (last_sep) {
        result.assign(manifest_path, (size_t)(last_sep - manifest_path + 1));
        result += data_file;
    } else {
        result = data_file;
    }

    return strdup(result.c_str());
}

/* 从持久化文件加载已保存的运行时值 */
static void load_persisted_values(const char* manifest_path)
{
    if (!manifest_path || !manifest_path[0]) return;
    char* data_path = resolve_data_file_path(manifest_path);
    if (!data_path) return;
    bs_config_persist_load_c(data_path);
    free(data_path);
}

// original constructor (no db)

AppSession::AppSession(const char* manifest_path)
{
    bootstrap_ctx(ctx_, io_, ok_);

    if (ok_ && manifest_path && manifest_path[0] != '\0')
    {
        bs_adapter_attach_ctx_open_persist_store(ctx_, manifest_path);
        load_persisted_values(manifest_path);
    }
}

// legacy constructor (single connection)

AppSession::AppSession(const char* manifest_path, const bs::db::DatabaseConfig* db_cfg)
{
    bootstrap_ctx(ctx_, io_, ok_);

    if (manifest_path && manifest_path[0] != '\0')
    {
        bs_adapter_attach_ctx_open_persist_store(ctx_, manifest_path);
        load_persisted_values(manifest_path);
    }

    if (ok_)
    {
        try_create_db(db_cfg);
    }
}

// new constructor (managed DbMgr with pool + health check + watcher)

AppSession::AppSession(const char* manifest_path, const bs::db::mgmt::DbMgrConfig* db_mgr_cfg)
{
    bootstrap_ctx(ctx_, io_, ok_);

    if (manifest_path && manifest_path[0] != '\0')
    {
        bs_adapter_attach_ctx_open_persist_store(ctx_, manifest_path);
        load_persisted_values(manifest_path);
    }

    if (ok_ && db_mgr_cfg)
    {
        try_open_db_mgr(db_mgr_cfg);
    }
}

void AppSession::try_create_db(const bs::db::DatabaseConfig* db_cfg)
{
    if (!db_cfg)
        return;

    if (db_cfg->driver_type == bs::db::DbDriverType::Null)
        return;

    bs::db::DbConnector* conn = bs::db::DbDriverFactory::Instance().Create(
        db_cfg->driver_type, *db_cfg);

    if (conn)
    {
        db_ = conn;
    }
}

void AppSession::try_open_db_mgr(const bs::db::mgmt::DbMgrConfig* cfg)
{
    if (!cfg)
        return;

    auto& mgr = bs::db::mgmt::DbMgr::Instance();
    if (!mgr.Open(*cfg))
        return;

    db_ = mgr.Acquire();
    if (db_)
    {
        using_db_mgr_ = true;
    }
}

// move / destroy

AppSession::AppSession(AppSession&& other) noexcept
    : ctx_(other.ctx_)
    , io_(other.io_)
    , db_(other.db_)
    , ok_(other.ok_)
    , using_db_mgr_(other.using_db_mgr_)
    , policy_gates_(std::move(other.policy_gates_))
    , custom_gates_(std::move(other.custom_gates_))
{
    other.ctx_ = nullptr;
    other.io_  = nullptr;
    other.db_  = nullptr;
    other.ok_  = false;
    other.using_db_mgr_ = false;
}

AppSession& AppSession::operator=(AppSession&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        ctx_             = other.ctx_;
        io_              = other.io_;
        db_              = other.db_;
        ok_              = other.ok_;
        using_db_mgr_    = other.using_db_mgr_;
        policy_gates_    = std::move(other.policy_gates_);
        custom_gates_    = std::move(other.custom_gates_);
        other.ctx_       = nullptr;
        other.io_        = nullptr;
        other.db_        = nullptr;
        other.ok_        = false;
        other.using_db_mgr_ = false;
    }
    return *this;
}

AppSession::~AppSession()
{
    destroy();
}

void AppSession::destroy()
{
    if (db_)
    {
        if (using_db_mgr_)
        {
            bs::db::mgmt::DbMgr::Instance().Release(db_);
        }
        else
        {
            delete db_;
        }
        db_ = nullptr;
    }
    if (io_)
    {
        bs_io_facade_destroy(io_);
        io_ = nullptr;
    }
    if (ctx_)
    {
        bs_adapter_attach_ctx_destroy(ctx_);
        ctx_ = nullptr;
    }
    ok_ = false;
    using_db_mgr_ = false;
}

void AppSession::registerGatePolicy(const std::string& gate_id, const ScenarioPolicy& policy)
{
    // upsert: 按 gate_id 定位，存在则替换，不存在则追加
    for (auto& existing : policy_gates_)
    {
        if (existing.gate_id == gate_id)
        {
            existing = policy;
            existing.gate_id = gate_id;
            return;
        }
    }
    ScenarioPolicy copy = policy;
    copy.gate_id = gate_id;
    policy_gates_.push_back(copy);
}

void AppSession::unregisterGatePolicy(const std::string& gate_id)
{
    policy_gates_.erase(
        std::remove_if(policy_gates_.begin(), policy_gates_.end(),
            [&gate_id](const ScenarioPolicy& p) { return p.gate_id == gate_id; }),
        policy_gates_.end());
}

void AppSession::registerGateCustom(const std::string& gate_id, const CustomGateEntry& entry)
{
    // upsert: 按 gate_id 定位
    for (auto& existing : custom_gates_)
    {
        if (existing.gate_id == gate_id)
        {
            existing = entry;
            existing.gate_id = gate_id;
            return;
        }
    }
    CustomGateEntry copy = entry;
    copy.gate_id = gate_id;
    custom_gates_.push_back(copy);
}

void AppSession::unregisterGateCustom(const std::string& gate_id)
{
    custom_gates_.erase(
        std::remove_if(custom_gates_.begin(), custom_gates_.end(),
            [&gate_id](const CustomGateEntry& e) { return e.gate_id == gate_id; }),
        custom_gates_.end());
}

} // namespace bs::app

/* ══════════════════════════════════════════════════════════════════
 * C ABI — Editor bridge: AppSession lifecycle
 *
 * app_session_create_c:  创建 AppSession，返回 opaque void* 指针
 * app_session_destroy_c: 销毁 AppSession
 * app_session_get_ctx_c: 获取 AttachContext*（用于 config_commit_batch_c）
 * app_session_is_ok_c:   检查 session 是否初始化成功
 * ══════════════════════════════════════════════════════════════════ */
extern "C" {

void* app_session_create_c(const char* manifest_path)
{
    auto* session = new bs::app::AppSession(manifest_path);
    if (!session->ok())
    {
        delete session;
        return nullptr;
    }
    return static_cast<void*>(session);
}

void app_session_destroy_c(void* session)
{
    if (!session) return;
    auto* s = static_cast<bs::app::AppSession*>(session);
    delete s;
}

void* app_session_get_ctx_c(void* session)
{
    if (!session) return nullptr;
    auto* s = static_cast<bs::app::AppSession*>(session);
    return static_cast<void*>(s->ctx());
}

int app_session_is_ok_c(void* session)
{
    if (!session) return 0;
    auto* s = static_cast<bs::app::AppSession*>(session);
    return s->ok() ? 1 : 0;
}

} // extern "C"
