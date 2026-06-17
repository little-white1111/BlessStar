#include "bs/app/sdk/app_session.h"

#include "bs/adapter/registry_bootstrap.h"
#include "bs/db/db_factory.h"
#include "bs/db/mgmt/db_mgr.h"
#include "bs/db/mgmt/db_mgr_config.h"

#include <cstdlib>

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

// original constructor (no db)

AppSession::AppSession(const char* manifest_path)
{
    bootstrap_ctx(ctx_, io_, ok_);

    if (ok_ && manifest_path && manifest_path[0] != '\0')
    {
        bs_adapter_attach_ctx_open_persist_store(ctx_, manifest_path);
    }
}

// legacy constructor (single connection)

AppSession::AppSession(const char* manifest_path, const bs::db::DatabaseConfig* db_cfg)
{
    bootstrap_ctx(ctx_, io_, ok_);

    if (manifest_path && manifest_path[0] != '\0')
    {
        bs_adapter_attach_ctx_open_persist_store(ctx_, manifest_path);
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

} // namespace bs::app
