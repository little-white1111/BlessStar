#ifndef BS_DB_MGMT_DB_POOL_H
#define BS_DB_MGMT_DB_POOL_H

#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"

#include <mutex>
#include <vector>

namespace bs::db::mgmt
{

class DbPool
{
public:
    explicit DbPool(int max_size);
    ~DbPool();

    DbPool(const DbPool&) = delete;
    DbPool& operator=(const DbPool&) = delete;

    bool Init(DbDriverType type, const DatabaseConfig& cfg);
    DbConnector* Acquire();
    void Release(DbConnector* conn);
    void Shutdown();
    int Available() const;
    int Size() const { return max_size_; }
    bool IsHealthy() const;

private:
    int                     max_size_;
    mutable std::mutex      mutex_;
    DbDriverType            driver_type_ = DbDriverType::Null;
    DatabaseConfig          cfg_;
    std::vector<DbConnector*> pool_;
    std::vector<DbConnector*> all_;
};

} // namespace bs::db::mgmt

#endif
