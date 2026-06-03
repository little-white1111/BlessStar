#include "bs/app/sdk/app_scenario_policy.h"

namespace bs::app
{
bool ValidateScenarioPolicy(const ScenarioPolicy& policy)
{
    if (policy.tenant.empty())
        return false;
    if (policy.max_batch <= 0 || policy.max_batch > 1024)
        return false;
    if (policy.type != ScenarioType::ExpenseReimburse && policy.type != ScenarioType::GlMapping)
        return false;
    return true;
}
} // namespace bs::app
