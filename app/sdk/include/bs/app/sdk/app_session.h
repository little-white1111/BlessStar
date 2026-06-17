#ifndef BS_APP_SDK_APP_SESSION_H
#define BS_APP_SDK_APP_SESSION_H

#include "bs/kernel/io/io.h"
#include "bs/adapter/attach_context.h"
#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/mgmt/db_mgr_config.h"

namespace bs::app
{

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
};

} // namespace bs::app

#endif // BS_APP_SDK_APP_SESSION_H
