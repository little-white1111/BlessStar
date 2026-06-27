#ifndef BS_APP_SDK_APP_SESSION_H
#define BS_APP_SDK_APP_SESSION_H

#include "bs/kernel/io/io.h"
#include "bs/adapter/attach_context.h"
#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/mgmt/db_mgr_config.h"
#include "bs/app/sdk/app_scenario_policy.h"
#include <vector>
#include <string>

namespace bs::app
{

/** App-level custom gate entry (not the adapter ReloadPathGateFn).
 *
 *  gate_id:     upsert/remove 的精确索引（第34天）
 *  ast_json:    create_gate_chain 编译产出的 AST JSON，由 bs_custom_gate_eval 解释执行
 *  fn:          指向通用 AST 求值器 bs_custom_gate_eval（所有 custom gate 共用）
 *  user_ctx:    指向 CustomGateEntry 自身，供求值器回查 ast_json
 */
struct CustomGateEntry
{
    std::string gate_id;
    std::string ast_json;
    int (*fn)(const void* data, size_t len, char* err, size_t err_cap, void* ctx);
    void* user_ctx;
};

class AppSession
{
public:
    explicit AppSession(const char* manifest_path = nullptr);

    AppSession(const char* manifest_path, const bs::db::DatabaseConfig* db_cfg);

    AppSession(const char* manifest_path, const bs::db::mgmt::DbMgrConfig* db_mgr_cfg);

    AppSession(const AppSession&)            = delete;
    AppSession& operator=(const AppSession&) = delete;

    AppSession(AppSession&& other) noexcept;
    AppSession& operator=(AppSession&& other) noexcept;

    ~AppSession();

    bool ok() const { return ok_; }

    AttachContext* ctx() { return ctx_; }
    const AttachContext* ctx() const { return ctx_; }

    IoFacade* io() { return io_; }
    const IoFacade* io() const { return io_; }

    bs::db::DbConnector* db() { return db_; }
    const bs::db::DbConnector* db() const { return db_; }

    bool using_db_mgr() const { return using_db_mgr_; }

    // ── Gate Registry（第34天 · GR-01：Gate 是基础设施）────────────────
    void registerGatePolicy(const std::string& gate_id, const ScenarioPolicy& policy);
    void registerGateCustom(const std::string& gate_id, const CustomGateEntry& entry);
    void unregisterGatePolicy(const std::string& gate_id);
    void unregisterGateCustom(const std::string& gate_id);
    const std::vector<ScenarioPolicy>& getPolicyGates() const { return policy_gates_; }
    const std::vector<CustomGateEntry>& getCustomGates() const { return custom_gates_; }

private:
    void bootstrap(const char* manifest_path);
    void try_create_db(const bs::db::DatabaseConfig* db_cfg);
    void try_open_db_mgr(const bs::db::mgmt::DbMgrConfig* cfg);
    void destroy();

    AttachContext*       ctx_ = nullptr;
    IoFacade*            io_  = nullptr;
    bs::db::DbConnector* db_  = nullptr;
    bool                 ok_  = false;
    bool                 using_db_mgr_ = false;

    // ── Gate Registry（第34天 · GR-01）─────────────────────────────────
    std::vector<ScenarioPolicy>  policy_gates_;
    std::vector<CustomGateEntry> custom_gates_;
};

} // namespace bs::app

#endif // BS_APP_SDK_APP_SESSION_H
