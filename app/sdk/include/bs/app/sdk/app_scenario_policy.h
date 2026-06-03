#ifndef BS_APP_SDK_APP_SCENARIO_POLICY_H
#define BS_APP_SDK_APP_SCENARIO_POLICY_H

/*
 * C-ST-7 contract block:
 * Thread safety: pure function; caller owns input/output objects.
 * Error semantics: returns false on invalid scenario policy.
 * Platform notes: N/A.
 */

#include <string>

namespace bs::app
{
enum class ScenarioType
{
    ExpenseReimburse = 0,
    GlMapping        = 1
};

struct ScenarioPolicy
{
    ScenarioType type = ScenarioType::ExpenseReimburse;
    std::string  tenant;
    bool         allow_hot_reload = true;
    int          max_batch        = 64;
};

bool ValidateScenarioPolicy(const ScenarioPolicy& policy);
} // namespace bs::app

#endif // BS_APP_SDK_APP_SCENARIO_POLICY_H
