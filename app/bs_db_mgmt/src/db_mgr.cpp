#include "bs/db/mgmt/db_mgr.h"
#include "bs/db/mgmt/db_plugin_loader.h"

namespace bs::db::mgmt {

DbMgr::~DbMgr() { Close(); }

bool DbMgr::Open(const DbMgrConfig& cfg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (opened_) return true;
    if (!cfg.plugin_dir.empty())
        DbPluginLoader::ScanAndLoad(cfg.plugin_dir, DbDriverFactory::Instance());
    pool_ = std::make_unique<DbPool>(cfg.pool_size);
    if (!pool_->Init(cfg.db_cfg.driver_type, cfg.db_cfg)) { pool_.reset(); return false; }
    auto factory = [&cfg]() -> bs::db::DbConnector* {
        return DbDriverFactory::Instance().Create(cfg.db_cfg.driver_type, cfg.db_cfg);
    };
    health_monitor_.Start(pool_.get(), cfg.health_check_interval_ms, factory);
    if (!cfg.watcher_config_path.empty() && cfg.config_change_callback)
        watcher_.Watch(cfg.watcher_config_path, 5000, cfg.config_change_callback);
    opened_ = true;
    return true;
}

bs::db::DbConnector* DbMgr::Acquire()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pool_) return nullptr;
    return pool_->Acquire();
}

void DbMgr::Release(bs::db::DbConnector* conn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_) pool_->Release(conn);
}

void DbMgr::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_) return;
    watcher_.Stop();
    health_monitor_.Stop();
    if (pool_) pool_->Shutdown();
    pool_.reset();
    opened_ = false;
}

} // namespace bs::db::mgmt
