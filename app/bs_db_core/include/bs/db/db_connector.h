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

    /** Human-readable driver name for logging. */
    virtual const char* DriverName() const = 0;

protected:
    explicit DbConnector(const DatabaseConfig& config) : config_(config) {}
    DatabaseConfig config_;
};

} // namespace bs::db

#endif // BS_DB_DB_CONNECTOR_H
