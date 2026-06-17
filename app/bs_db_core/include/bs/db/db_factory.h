#ifndef BS_DB_DB_FACTORY_H
#define BS_DB_DB_FACTORY_H

#include <map>
#include <mutex>

#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"

namespace bs::db
{

class DbDriverFactory
{
public:
    using CreatorFn = DbConnector*(*)(const DatabaseConfig&);

    static DbDriverFactory& Instance()
    {
        static DbDriverFactory inst;
        return inst;
    }

    bool Register(DbDriverType key, CreatorFn fn)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return registry_.emplace(key, fn).second;
    }

    DbConnector* Create(DbDriverType key, const DatabaseConfig& config) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(key);
        if (it != registry_.end())
            return it->second(config);
        return nullptr;
    }

    bool Has(DbDriverType key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return registry_.find(key) != registry_.end();
    }

private:
    DbDriverFactory() = default;
    mutable std::mutex mutex_;
    std::map<DbDriverType, CreatorFn> registry_;
};

// Auto-register null driver at static init time.
extern bool RegisterNullDbDriver();

} // namespace bs::db

#endif // BS_DB_DB_FACTORY_H
