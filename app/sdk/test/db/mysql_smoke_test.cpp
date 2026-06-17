#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"
#include <mysql.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static const char* SMOKE_HOST = "localhost";
static const char* SMOKE_USER = "root";
static const char* SMOKE_PASS = "123456";
static const char* SMOKE_DB   = "blessstar_smoke_test";

static void exec_sql(MYSQL* db, const char* sql)
{
    int rc = mysql_query(db, sql);
    if (rc != 0)
    {
        std::cerr << "[ERROR] mysql_query failed: " << mysql_error(db) << std::endl;
        std::cerr << "  SQL: " << sql << std::endl;
        std::exit(1);
    }
}

int main()
{
    // 0. Connect without DB to create test database
    {
        MYSQL* db = mysql_init(nullptr);
        assert(db != nullptr);
        MYSQL* rc = mysql_real_connect(db, SMOKE_HOST, SMOKE_USER, SMOKE_PASS,
                                        nullptr, 0, nullptr, 0);
        assert(rc != nullptr && "mysql_real_connect (no DB) failed");
        exec_sql(db, "CREATE DATABASE IF NOT EXISTS blessstar_smoke_test");
        mysql_close(db);
    }

    // 1. Connect to test DB
    MYSQL* db = nullptr;
    {
        db = mysql_init(nullptr);
        assert(db != nullptr);
        MYSQL* rc = mysql_real_connect(db, SMOKE_HOST, SMOKE_USER, SMOKE_PASS,
                                        SMOKE_DB, 0, nullptr, 0);
        assert(rc != nullptr && "mysql_real_connect failed");
    }

    // 2. Create vendor_configs table + insert test data
    exec_sql(db, "DROP TABLE IF EXISTS vendor_configs");
    exec_sql(db,
        "CREATE TABLE vendor_configs ("
        "  tenant VARCHAR(128),"
        "  config_id VARCHAR(128),"
        "  v1_json TEXT,"
        "  PRIMARY KEY(tenant, config_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    exec_sql(db,
        "INSERT INTO vendor_configs VALUES("
        "'t1','c1','{\"kernel_version\":\"v1\",\"instructions\":{}}')");

    exec_sql(db,
        "INSERT INTO vendor_configs VALUES("
        "'tenant_A','cfg_001','{\"kernel_version\":\"v1\",\"instructions\":{\"action\":\"update\"}}')");

    mysql_close(db);

    // 3. Use BlessStar DbDriverFactory to test the connector
    bs::db::DatabaseConfig cfg;
    cfg.driver_type = bs::db::DbDriverType::MySQL;
    cfg.dsn         = SMOKE_HOST;
    cfg.user        = SMOKE_USER;
    cfg.password    = SMOKE_PASS;
    cfg.database    = SMOKE_DB;

    assert(bs::db::DbDriverFactory::Instance().Has(bs::db::DbDriverType::MySQL)
           && "MySQL driver should be registered (static init)");

    bs::db::DbConnector* conn = bs::db::DbDriverFactory::Instance().Create(
        bs::db::DbDriverType::MySQL, cfg);
    assert(conn != nullptr && "MySQL DbConnector creation should succeed");
    assert(std::string(conn->DriverName()) == "mysql");

    // 4. Fetch known row
    {
        std::vector<std::uint8_t> data;
        std::string err;
        bool ok = conn->FetchConfig("t1", "c1", &data, &err);
        assert(ok && "FetchConfig should succeed");
        assert(err.empty());
        std::string json(data.begin(), data.end());
        assert(json.find("kernel_version") != std::string::npos);
        std::cout << "  t1/c1 => " << json << std::endl;
    }

    // 5. Fetch another row
    {
        std::vector<std::uint8_t> data;
        std::string err;
        bool ok = conn->FetchConfig("tenant_A", "cfg_001", &data, &err);
        assert(ok && "FetchConfig for tenant_A should succeed");
        std::string json(data.begin(), data.end());
        assert(json.find("action") != std::string::npos);
        std::cout << "  tenant_A/cfg_001 => " << json << std::endl;
    }

    // 6. Fetch nonexistent -> should fail
    {
        std::vector<std::uint8_t> data;
        std::string err;
        bool ok = conn->FetchConfig("no_tenant", "no_id", &data, &err);
        assert(!ok && "FetchConfig for missing key should fail");
        std::cout << "  no_tenant/no_id => correctly not found" << std::endl;
    }

    // 7. Batch fetch
    {
        std::vector<std::pair<std::string, std::string>> pairs = {
            {"t1", "c1"},
            {"tenant_A", "cfg_001"}
        };
        std::vector<std::vector<std::uint8_t>> results;
        std::string err;
        bool ok = conn->FetchConfigs(pairs, &results, &err);
        assert(ok && "FetchConfigs should succeed");
        assert(results.size() == 2);
        std::cout << "  FetchConfigs batch => " << results.size() << " results" << std::endl;
    }

    // 8. Batch with missing key -> should fail
    {
        std::vector<std::pair<std::string, std::string>> pairs = {
            {"t1", "c1"},
            {"bad", "key"}
        };
        std::vector<std::vector<std::uint8_t>> results;
        std::string err;
        bool ok = conn->FetchConfigs(pairs, &results, &err);
        assert(!ok);
        std::cout << "  FetchConfigs with missing key => correctly failed" << std::endl;
    }

    // 9. Cleanup: drop test DB
    delete conn;

    {
        MYSQL* cleanup_db = mysql_init(nullptr);
        assert(cleanup_db != nullptr);
        MYSQL* rc = mysql_real_connect(cleanup_db, SMOKE_HOST, SMOKE_USER, SMOKE_PASS,
                                        nullptr, 0, nullptr, 0);
        if (rc)
        {
            exec_sql(cleanup_db, "DROP DATABASE IF EXISTS blessstar_smoke_test");
            mysql_close(cleanup_db);
        }
    }

    std::cout << "[PASS] MySQL smoke test (bs_db_core) passed." << std::endl;
    return 0;
}
