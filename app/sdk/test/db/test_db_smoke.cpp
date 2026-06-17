#include "bs/app/sdk/db/db_config.h"
#include "bs/app/sdk/db/db_factory.h"
#include "bs/app/sdk/app_session.h"
#include <sqlite3.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>

int main()
{
    // 1. Create temporary SQLite DB file
    const char* db_path = ".temp_schema.sqlite";
    {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path, &db);
        assert(rc == SQLITE_OK && "Failed to open sqlite descriptor");
        const char* create_tbl =
            "CREATE TABLE IF NOT EXISTS vendor_configs "
            "(tenant TEXT, config_id TEXT, v1_json TEXT, "
            "PRIMARY KEY(tenant, config_id));";
        char* errmsg = nullptr;
        rc = sqlite3_exec(db, create_tbl, nullptr, nullptr, &errmsg);
        assert(rc == SQLITE_OK && "Failed create table");
        if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }
        // Insert a dummy row
        const char* ins =
            "INSERT INTO vendor_configs VALUES"
            "('t1','c1','{\\\"kernel_version\\\":\\\"v1\\\",\\\"instructions\\\":{}}');";
        rc = sqlite3_exec(db, ins, nullptr, nullptr, &errmsg);
        assert(rc == SQLITE_OK && "Failed insert row");
        if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }
        sqlite3_close(db);
    }

    // 2. Build DatabaseConfig (SQLite)
    bs::app::DatabaseConfig cfg;
    cfg.driver_type = bs::app::DbDriverType::SQLite;
    cfg.dsn = db_path;

    // 3. Create AppSession with DB driver
    bs::app::AppSession session(nullptr, &cfg);
    assert(session.ok() && "AppSession bootstrap should succeed");
    assert(session.db() && "DbConnector should be created");

    // 4. Fetch a known config
    std::vector<std::uint8_t> data;
    std::string err;
    bool ok = session.db()->FetchConfig("t1", "c1", &data, &err);
    assert(ok && err.empty() && "FetchConfig should succeed");
    std::string json(data.begin(), data.end());
    assert(json.find("kernel_version") != std::string::npos &&
           "Result should contain kernel_version");

    // 5. Fetch nonexistent config → should fail gracefully
    std::vector<std::string> not_found;
    std::string err2;
    ok = session.db()->FetchConfig("no_tenant", "no_id", nullptr, &err2);
    // Expect failure
    assert(!ok);

    // 6. Cleanup temp DB
    std::filesystem::remove(db_path);

    std::cout << "SQLite smoke test passed." << std::endl;
    return 0;
}
