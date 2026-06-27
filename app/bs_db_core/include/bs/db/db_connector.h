#ifndef BS_DB_DB_CONNECTOR_H
#define BS_DB_DB_CONNECTOR_H

/*
 * C-ST-7 contract block:
 * Thread safety: not thread-safe; caller must serialize access.
 * Error semantics: FetchConfig/FetchConfigs return false and fill out_error.
 * Platform notes: Pure virtual; concrete implementations loaded via DbDriverFactory.
 */

#include "bs/db/db_config.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace bs::db
{

class DbConnector
{
public:
    virtual ~DbConnector() = default;

    /**
     * Fetch a single config by tenant + config_id. Returns raw bytes.
     * Returns true on success; false + out_error on failure.
     */
    virtual bool FetchConfig(const char* tenant, const char* config_id,
                             std::vector<std::uint8_t>* out_data,
                             std::string* out_error) = 0;

    /**
     * Batch fetch. Returns true if all succeeded; false + out_error on first failure.
     */
    virtual bool FetchConfigs(
        const std::vector<std::pair<std::string, std::string>>& tenant_id_pairs,
        std::vector<std::vector<std::uint8_t>>* out_results,
        std::string* out_error) = 0;

    /**
     * Execute a parameterized SQL query.
     * @param sql       SQL statement with '?' placeholders
     * @param params    Parameter values in order (stringified)
     * @param out_rows  Output: flat vector of all row values (row-major)
     * @param out_cols  Output: column names
     * @param out_error Error message on failure
     * @return true on success, false on failure
     */
    virtual bool ExecuteQuery(const char* sql,
                              const std::vector<std::string>& params,
                              std::vector<std::vector<std::string>>* out_rows,
                              std::vector<std::string>* out_cols,
                              std::string* out_error)
    {
        (void)sql; (void)params; (void)out_rows; (void)out_cols;
        if (out_error)
            *out_error = "ExecuteQuery not implemented by this driver";
        return false;
    }

    /** Human-readable driver name for logging. */
    virtual const char* DriverName() const = 0;

protected:
    explicit DbConnector(const DatabaseConfig& config) : config_(config) {}
    DatabaseConfig config_;
};

} // namespace bs::db

#endif // BS_DB_DB_CONNECTOR_H
