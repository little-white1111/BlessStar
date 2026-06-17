#ifndef BS_DB_MGMT_DB_MGR_H
#define BS_DB_MGMT_DB_MGR_H

#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"
#include "bs/db/mgmt/db_mgr_config.h"
#include "bs/db/mgmt/db_pool.h"
#include "bs/db/mgmt/db_health_monitor.h"
#include "bs/db/mgmt/db_watcher.h"

#include <memory>
#include <mutex>

namespace bs::db::mgmt
{

class DbMgr
{
public:
    static DbMgr& Instance()
    {
        static DbMgr inst;
        return inst;
    }

    bool Open(const DbMgrConfig& cfg);
    DbConnector* Acquire();
    void Release(DbConnector* conn);
    void Close();

    bool IsOpen() const { return opened_; }
    DbPool* pool() { return pool_.get(); }

private:
    DbMgr() = default;
    ~DbMgr();

    DbMgr(const DbMgr&) = delete;
    DbMgr& operator=(const DbMgr&) = delete;

    std::mutex                      mutex_;
    bool                            opened_ = false;
    std::unique_ptr<DbPool>         pool_;
    DbHealthMonitor                 health_monitor_;
    DbWatcher                       watcher_;
};

} // namespace bs::db::mgmt

#endif
