#include "bs/db/mgmt/db_pool.h"

namespace bs::db::mgmt {

DbPool::DbPool(int max_size) : max_size_(max_size) {}

DbPool::~DbPool() { Shutdown(); }

bool DbPool::Init(DbDriverType type, const DatabaseConfig& cfg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    driver_type_ = type;
    cfg_ = cfg;
    for (int i = 0; i < max_size_; ++i) {
        bs::db::DbConnector* conn = DbDriverFactory::Instance().Create(type, cfg);
        if (conn) { pool_.push_back(conn); all_.push_back(conn); }
    }
    return !pool_.empty();
}

bs::db::DbConnector* DbPool::Acquire()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_.empty()) return nullptr;
    bs::db::DbConnector* conn = pool_.back();
    pool_.pop_back();
    return conn;
}

void DbPool::Release(bs::db::DbConnector* conn)
{
    if (!conn) return;
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push_back(conn);
}

void DbPool::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* c : all_) delete c;
    all_.clear(); pool_.clear();
}

int DbPool::Available() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(pool_.size());
}

bool DbPool::IsHealthy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !pool_.empty();
}

} // namespace bs::db::mgmt
