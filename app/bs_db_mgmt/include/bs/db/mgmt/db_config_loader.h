#ifndef BS_DB_MGMT_DB_CONFIG_LOADER_H
#define BS_DB_MGMT_DB_CONFIG_LOADER_H

#include "bs/db/db_config.h"
#include "bs/db/mgmt/db_mgr_config.h"

#include <string>

namespace bs::db::mgmt
{

struct DbConfigLoader
{
    static DbMgrConfig FromFile(const std::string& path);
    static DbMgrConfig FromJson(const std::string& json);
};

} // namespace bs::db::mgmt

#endif
