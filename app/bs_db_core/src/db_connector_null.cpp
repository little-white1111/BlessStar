#include "bs/db/db_config.h"
#include "bs/db/db_connector.h"
#include "bs/db/db_factory.h"

namespace bs::db
{

namespace
{

class NullDbConnector final : public DbConnector
{
public:
    explicit NullDbConnector(const DatabaseConfig& config)
        : DbConnector(config)
    {
    }

    bool FetchConfig(const char* /*tenant*/, const char* /*config_id*/,
                     std::vector<std::uint8_t>* /*out_data*/,
                     std::string* out_error) override
    {
        if (out_error)
            *out_error = "null driver: DB not configured";
        return false;
    }

    bool FetchConfigs(
        const std::vector<std::pair<std::string, std::string>>& /*tenant_id_pairs*/,
        std::vector<std::vector<std::uint8_t>>* /*out_results*/,
        std::string* out_error) override
    {
        if (out_error)
            *out_error = "null driver: DB not configured";
        return false;
    }

    const char* DriverName() const override { return "null"; }
};

} // namespace

static bool g_null_registered = []() -> bool {
    DbDriverFactory::Instance().Register(
        DbDriverType::Null,
        [](const DatabaseConfig& cfg) -> DbConnector* {
            return new NullDbConnector(cfg);
        });
    return true;
}();

bool RegisterNullDbDriver()
{
    return g_null_registered;
}

} // namespace bs::db
