#ifndef BS_DB_MGMT_DB_HEALTH_MONITOR_H
#define BS_DB_MGMT_DB_HEALTH_MONITOR_H

#include "bs/db/db_connector.h"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace bs::db::mgmt
{

class DbPool;

class DbHealthMonitor
{
public:
    using ConnectorFactory = std::function<DbConnector*()>;

    DbHealthMonitor();
    ~DbHealthMonitor();

    DbHealthMonitor(const DbHealthMonitor&) = delete;
    DbHealthMonitor& operator=(const DbHealthMonitor&) = delete;

    void Start(DbPool* pool, int interval_ms, ConnectorFactory factory);
    void Stop();

    bool running() const { return running_; }

private:
    void loop();

    std::atomic<bool>   running_{false};
    std::unique_ptr<std::thread> thread_;
    DbPool*             pool_ = nullptr;
    int                 interval_ms_ = 30000;
    ConnectorFactory    connector_factory_;
};

} // namespace bs::db::mgmt

#endif
