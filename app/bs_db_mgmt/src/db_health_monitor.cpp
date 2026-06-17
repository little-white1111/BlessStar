#include "bs/db/mgmt/db_health_monitor.h"
#include "bs/db/mgmt/db_pool.h"

namespace bs::db::mgmt {

DbHealthMonitor::DbHealthMonitor() = default;
DbHealthMonitor::~DbHealthMonitor() { Stop(); }

void DbHealthMonitor::Start(DbPool* pool, int interval_ms, ConnectorFactory factory)
{
    if (running_) return;
    pool_ = pool;
    interval_ms_ = interval_ms;
    connector_factory_ = std::move(factory);
    running_ = true;
    thread_ = std::make_unique<std::thread>(&DbHealthMonitor::loop, this);
}

void DbHealthMonitor::Stop()
{
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
    thread_.reset();
}

void DbHealthMonitor::loop()
{
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
        if (!running_) break;
        if (pool_ && connector_factory_) {
            bs::db::DbConnector* probe = connector_factory_();
            if (probe) {
                std::string err;
                std::vector<std::uint8_t> data;
                probe->FetchConfig("__health__", "__check__", &data, &err);
                delete probe;
            }
        }
    }
}

} // namespace bs::db::mgmt
