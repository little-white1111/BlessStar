#ifndef BS_DB_DB_CONFIG_H
#define BS_DB_DB_CONFIG_H

/*
 * C-ST-7 contract block:
 * Thread safety: DatabaseConfig is a value type; not thread-safe by itself.
 * Error semantics: Create() returns nullptr on validation failure; check bs_log.
 * Platform notes: DB configuration; password read from BS_DB_PASSWORD env var.
 */

#include <string>

namespace bs::db
{

enum class DbDriverType : int {
    Null = 0,
    PostgreSQL,
    MySQL,
    SQLite,
    Custom = 999
};

struct DatabaseConfig {
    DbDriverType driver_type = DbDriverType::Null;
    std::string  dsn;         // host (SQLite: file path)
    std::string  user;        // username
    std::string  password;    // password
    std::string  database;    // database name
    int          pool_size   = 4;
    int          timeout_ms  = 5000;

    static constexpr int kMaxPoolSize  = 64;
    static constexpr int kMaxTimeoutMs = 60000;
    static constexpr int kMinTimeoutMs = 100;
};

} // namespace bs::db

#endif // BS_DB_DB_CONFIG_H
