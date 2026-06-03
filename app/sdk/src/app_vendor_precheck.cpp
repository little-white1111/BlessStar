#include <string_view>

#include "bs/app/sdk/app_vendor_precheck.h"

namespace bs::app
{

bool PrecheckV1BytesForScenario(const std::uint8_t* data, std::size_t len,
                                const ScenarioPolicy& policy, std::string* error_out)
{
    if (!data || len == 0)
    {
        if (error_out)
            *error_out = "empty v1 payload";
        return false;
    }
    const std::string_view view(reinterpret_cast<const char*>(data), len);
    if (view.find("\"kernel_version\"") == std::string_view::npos ||
        view.find("\"instructions\"") == std::string_view::npos)
    {
        if (error_out)
            *error_out = "missing v1 markers (kernel_version/instructions)";
        return false;
    }
    if (!ValidateScenarioPolicy(policy))
    {
        if (error_out)
            *error_out = "invalid scenario policy";
        return false;
    }
    return true;
}

} // namespace bs::app
