#ifndef BS_DB_MGMT_DB_MGR_CONFIG_H
#define BS_DB_MGMT_DB_MGR_CONFIG_H

#include "bs/db/db_config.h"

#include <functional>
#include <string>

namespace bs::db::mgmt
{

struct DbMgrConfig {
    bs::db::DatabaseConfig db_cfg;

    int     pool_size               = 4;
    int     health_check_interval_ms = 30000;
    bool    auto_reconnect          = true;
    int     max_retries             = 3;

    std::string plugin_dir;            // e.g. "drivers/"
    std::string watcher_config_path;   // file path to watch for changes
    std::function<void()> config_change_callback;
};

} // namespace bs::db::mgmt

#endif // BS_DB_MGMT_DB_MGR_CONFIG_H
