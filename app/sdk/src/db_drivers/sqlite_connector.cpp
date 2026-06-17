#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"
#include <sqlite3.h>
#include <cstring>
#include <string>

namespace bs::db
{

class SqliteDbConnector final : public DbConnector
{
    sqlite3* db_ = nullptr;
public:
    explicit SqliteDbConnector(const DatabaseConfig& config) : DbConnector(config)
    {
        int rc = sqlite3_open(config.dsn.c_str(), &db_);
        if (rc != SQLITE_OK)
        {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    ~SqliteDbConnector() override
    {
        if (db_)
            sqlite3_close(db_);
    }

    bool FetchConfig(const char* tenant, const char* config_id,
                     std::vector<std::uint8_t>* out_data,
                     std::string* out_error) override
    {
        if (!db_)
        {
            if (out_error)
                *out_error = "sqlite: not connected";
            return false;
        }

        std::string sql = "SELECT v1_json FROM vendor_configs WHERE tenant=? AND config_id=?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            if (out_error)
                *out_error = sqlite3_errmsg(db_);
            return false;
        }

        sqlite3_bind_text(stmt, 1, tenant, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, config_id, -1, SQLITE_STATIC);

        bool ok = false;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int len = sqlite3_column_bytes(stmt, 0);
            if (out_data && blob && len > 0)
            {
                out_data->assign(static_cast<const std::uint8_t*>(blob),
                                 static_cast<const std::uint8_t*>(blob) + len);
                ok = true;
            }
            else
                ok = (blob == nullptr && len == 0);
        }

        sqlite3_finalize(stmt);
        return ok;
    }

    bool FetchConfigs(const std::vector<std::pair<std::string, std::string>>& tenant_id_pairs,
                      std::vector<std::vector<std::uint8_t>>* out_results,
                      std::string* out_error) override
    {
        if (!db_)
        {
            if (out_error)
                *out_error = "sqlite: not connected";
            return false;
        }
        out_results->clear();
        for (const auto& pair : tenant_id_pairs)
        {
            std::vector<std::uint8_t> data;
            bool ok = FetchConfig(pair.first.c_str(), pair.second.c_str(), &data, out_error);
            if (!ok)
                return false;
            out_results->push_back(std::move(data));
        }
        return true;
    }

    const char* DriverName() const override { return "sqlite"; }
};

static bool sqlite_driver_registered = []() -> bool
{
    DbDriverFactory::Instance().Register(DbDriverType::SQLite,
        [](const DatabaseConfig& cfg) -> DbConnector* {
            return new SqliteDbConnector(cfg);
        });
    return true;
}();

} // namespace bs::db
