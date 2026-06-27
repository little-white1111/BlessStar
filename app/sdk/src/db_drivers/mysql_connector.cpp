#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"
#include <mysql.h>
#include <cstring>
#include <string>
#include <vector>

namespace bs::db
{

class MysqlDbConnector final : public DbConnector
{
    MYSQL* conn_ = nullptr;
public:
    explicit MysqlDbConnector(const DatabaseConfig& config) : DbConnector(config)
    {
        conn_ = mysql_init(nullptr);
        if (!conn_)
            return;
        mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
        if (!mysql_real_connect(conn_,
                                config.dsn.c_str(),
                                config.user.c_str(),
                                config.password.c_str(),
                                config.database.c_str(),
                                0,
                                nullptr,
                                CLIENT_MULTI_STATEMENTS))
        {
            mysql_close(conn_);
            conn_ = nullptr;
        }
    }

    ~MysqlDbConnector() override
    {
        if (conn_)
        {
            mysql_close(conn_);
            conn_ = nullptr;
        }
    }

    bool FetchConfig(const char* tenant, const char* config_id,
                     std::vector<std::uint8_t>* out_data,
                     std::string* out_error) override
    {
        if (!conn_)
        {
            if (out_error)
                *out_error = "mysql: not connected";
            return false;
        }

        MYSQL_STMT* stmt = mysql_stmt_init(conn_);
        if (!stmt)
        {
            if (out_error)
                *out_error = mysql_error(conn_);
            return false;
        }
        const char* sql = "SELECT v1_json FROM vendor_configs WHERE tenant=? AND config_id=?";
        if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0)
        {
            if (out_error)
                *out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }

        MYSQL_BIND bind[2];
        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = const_cast<char*>(tenant);
        bind[0].buffer_length = static_cast<unsigned long>(strlen(tenant));
        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = const_cast<char*>(config_id);
        bind[1].buffer_length = static_cast<unsigned long>(strlen(config_id));

        if (mysql_stmt_bind_param(stmt, bind) != 0)
        {
            if (out_error)
                *out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }

        if (mysql_stmt_execute(stmt) != 0)
        {
            if (out_error)
                *out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }

        MYSQL_BIND result_bind;
        memset(&result_bind, 0, sizeof(result_bind));
        char result_buf[65536] = {0};
        unsigned long result_len = 0;
        bool is_null = false;
        result_bind.buffer_type = MYSQL_TYPE_LONG_BLOB;
        result_bind.buffer = result_buf;
        result_bind.buffer_length = sizeof(result_buf);
        result_bind.length = &result_len;
        result_bind.is_null = &is_null;

        if (mysql_stmt_bind_result(stmt, &result_bind) != 0)
        {
            if (out_error)
                *out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }

        int rc = mysql_stmt_fetch(stmt);
        if (rc == MYSQL_NO_DATA)
        {
            mysql_stmt_close(stmt);
            return false;
        }
        else if (rc != 0)
        {
            if (out_error)
                *out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }

        if (out_data && !is_null && result_len > 0)
        {
            out_data->assign(reinterpret_cast<const std::uint8_t*>(result_buf),
                             reinterpret_cast<const std::uint8_t*>(result_buf) + result_len);
        }
        mysql_stmt_close(stmt);
        return true;
    }

    bool FetchConfigs(const std::vector<std::pair<std::string, std::string>>& tenant_id_pairs,
                      std::vector<std::vector<std::uint8_t>>* out_results,
                      std::string* out_error) override
    {
        if (!conn_)
        {
            if (out_error)
                *out_error = "mysql: not connected";
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

    bool ExecuteQuery(const char* sql,
                      const std::vector<std::string>& params,
                      std::vector<std::vector<std::string>>* out_rows,
                      std::vector<std::string>* out_cols,
                      std::string* out_error) override
    {
        if (!conn_)
        {
            if (out_error) *out_error = "mysql: not connected";
            return false;
        }
        MYSQL_STMT* stmt = mysql_stmt_init(conn_);
        if (!stmt)
        {
            if (out_error) *out_error = mysql_error(conn_);
            return false;
        }
        if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0)
        {
            if (out_error) *out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }
        // Bind parameters
        if (!params.empty())
        {
            std::vector<MYSQL_BIND> param_bind(params.size());
            std::vector<unsigned long> param_lens(params.size());
            memset(param_bind.data(), 0, param_bind.size() * sizeof(MYSQL_BIND));
            for (size_t i = 0; i < params.size(); ++i)
            {
                param_bind[i].buffer_type = MYSQL_TYPE_STRING;
                param_bind[i].buffer = const_cast<char*>(params[i].c_str());
                param_lens[i] = static_cast<unsigned long>(params[i].size());
                param_bind[i].length = &param_lens[i];
            }
            if (mysql_stmt_bind_param(stmt, param_bind.data()) != 0)
            {
                if (out_error) *out_error = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                return false;
            }
        }
        if (mysql_stmt_execute(stmt) != 0)
        {
            if (out_error) *out_error = mysql_stmt_error(stmt);
            mysql_stmt_close(stmt);
            return false;
        }
        // Get column count
        MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
        int col_count = meta ? mysql_num_fields(meta) : 0;
        if (out_cols && meta)
        {
            out_cols->clear();
            for (int c = 0; c < col_count; ++c)
            {
                MYSQL_FIELD* field = mysql_fetch_field_direct(meta, c);
                out_cols->push_back(field ? field->name : "");
            }
        }
        // Bind result buffers
        std::vector<MYSQL_BIND> result_bind(col_count);
        std::vector<std::vector<char>> result_bufs(col_count);
        std::vector<unsigned long> result_lens(col_count);
        std::vector<bool> result_nulls(col_count);
        memset(result_bind.data(), 0, result_bind.size() * sizeof(MYSQL_BIND));
        for (int c = 0; c < col_count; ++c)
        {
            result_bufs[c].resize(65536, '\0');
            result_bind[c].buffer_type = MYSQL_TYPE_STRING;
            result_bind[c].buffer = result_bufs[c].data();
            result_bind[c].buffer_length = 65536;
            result_bind[c].length = &result_lens[c];
            result_bind[c].is_null = reinterpret_cast<bool*>(&result_nulls[c]);
        }
        if (col_count > 0 && mysql_stmt_bind_result(stmt, result_bind.data()) != 0)
        {
            if (out_error) *out_error = mysql_stmt_error(stmt);
            if (meta) mysql_free_result(meta);
            mysql_stmt_close(stmt);
            return false;
        }
        // Fetch rows
        if (out_rows) out_rows->clear();
        while (mysql_stmt_fetch(stmt) == 0)
        {
            std::vector<std::string> row;
            for (int c = 0; c < col_count; ++c)
            {
                if (result_nulls[c])
                    row.push_back("");
                else
                    row.push_back(std::string(result_bufs[c].data(), result_lens[c]));
            }
            if (out_rows) out_rows->push_back(std::move(row));
        }
        if (meta) mysql_free_result(meta);
        mysql_stmt_close(stmt);
        return true;
    }

    const char* DriverName() const override { return "mysql"; }
};

static bool mysql_driver_registered = []() -> bool
{
    DbDriverFactory::Instance().Register(DbDriverType::MySQL,
        [](const DatabaseConfig& cfg) -> DbConnector* {
            return new MysqlDbConnector(cfg);
        });
    return true;
}();

} // namespace bs::db
