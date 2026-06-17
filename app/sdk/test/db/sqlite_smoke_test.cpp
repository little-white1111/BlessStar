#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"
#include <sqlite3.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    const char* db_path = "smoke_test.sqlite";

    // 1. Create temp SQLite DB with vendor_configs table + two rows
    {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path, &db);
        assert(rc == SQLITE_OK && "sqlite3_open failed");
        char* errmsg = nullptr;
        rc = sqlite3_exec(db,
            "CREATE TABLE IF NOT EXISTS vendor_configs ("
            "tenant TEXT, config_id TEXT, v1_json TEXT, "
            "PRIMARY KEY(tenant, config_id));",
            nullptr, nullptr, &errmsg);
        assert(rc == SQLITE_OK && "CREATE TABLE failed");
        if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }

        rc = sqlite3_exec(db,
            "INSERT OR REPLACE INTO vendor_configs VALUES("
            "'t1','c1','{\"kernel_version\":\"v1\",\"instructions\":{}}');",
            nullptr, nullptr, &errmsg);
        assert(rc == SQLITE_OK && "INSERT failed");
        if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }

        rc = sqlite3_exec(db,
            "INSERT OR REPLACE INTO vendor_configs VALUES("
            "'tenant_A','cfg_001','{\"kernel_version\":\"v1\",\"instructions\":{\"action\":\"update\"}}');",
            nullptr, nullptr, &errmsg);
        assert(rc == SQLITE_OK && "INSERT row2 failed");
        if (errmsg) { sqlite3_free(errmsg); errmsg = nullptr; }

        sqlite3_close(db);
    }

    // 2. Build DatabaseConfig pointing to the temp DB
    bs::db::DatabaseConfig cfg;
    cfg.driver_type = bs::db::DbDriverType::SQLite;
    cfg.dsn = db_path;

    // 3. Create DbConnector via bs_db_core factory (no bs_app_sdk needed)
    assert(bs::db::DbDriverFactory::Instance().Has(bs::db::DbDriverType::SQLite)
           && "SQLite driver should be registered (static init)");

    bs::db::DbConnector* conn = bs::db::DbDriverFactory::Instance().Create(
        bs::db::DbDriverType::SQLite, cfg);
    assert(conn != nullptr && "DbConnector creation should succeed");
    assert(std::string(conn->DriverName()) == "sqlite");

    // 4. Fetch the known config row
    {
        std::vector<std::uint8_t> data;
        std::string err;
        bool ok = conn->FetchConfig("t1", "c1", &data, &err);
        assert(ok && "FetchConfig should succeed");
        assert(err.empty() && "Error string should be empty");
        std::string json(data.begin(), data.end());
        assert(json.find("kernel_version") != std::string::npos
               && "Result should contain kernel_version");
    }

    // 5. Fetch another known row
    {
        std::vector<std::uint8_t> data;
        std::string err;
        bool ok = conn->FetchConfig("tenant_A", "cfg_001", &data, &err);
        assert(ok && "FetchConfig for tenant_A should succeed");
        std::string json(data.begin(), data.end());
        assert(json.find("action") != std::string::npos
               && "Result should contain action field");
    }

    // 6. Fetch nonexistent key -> should fail gracefully
    {
        std::vector<std::uint8_t> data;
        std::string err;
        bool ok = conn->FetchConfig("no_tenant", "no_id", &data, &err);
        assert(!ok && "FetchConfig for missing key should fail");
    }

    // 7. Batch fetch (FetchConfigs)
    {
        std::vector<std::pair<std::string, std::string>> pairs = {
            {"t1", "c1"},
            {"tenant_A", "cfg_001"}
        };
        std::vector<std::vector<std::uint8_t>> results;
        std::string err;
        bool ok = conn->FetchConfigs(pairs, &results, &err);
        assert(ok && "FetchConfigs should succeed");
        assert(results.size() == 2 && "Should return 2 results");
        assert(results[0].size() > 0 && "First result should be non-empty");
        assert(results[1].size() > 0 && "Second result should be non-empty");
    }

    // 8. Batch fetch with a missing key -> should fail
    {
        std::vector<std::pair<std::string, std::string>> pairs = {
            {"t1", "c1"},
            {"bad", "key"}
        };
        std::vector<std::vector<std::uint8_t>> results;
        std::string err;
        bool ok = conn->FetchConfigs(pairs, &results, &err);
        assert(!ok && "FetchConfigs with missing key should fail");
    }

    // 9. Cleanup
    delete conn;
    std::filesystem::remove(db_path);

    std::cout << "[PASS] SQLite smoke test (bs_db_core) passed." << std::endl;
    return 0;
}
