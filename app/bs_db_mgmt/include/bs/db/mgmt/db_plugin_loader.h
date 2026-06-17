#ifndef BS_DB_MGMT_DB_PLUGIN_LOADER_H
#define BS_DB_MGMT_DB_PLUGIN_LOADER_H

#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"

#include <string>

namespace bs::db::mgmt
{

class DbPluginLoader
{
public:
    static int ScanAndLoad(const std::string& dir, DbDriverFactory& factory);
    static bool LoadOne(const std::string& plugin_path, DbDriverFactory& factory);
};

} // namespace bs::db::mgmt

#endif
