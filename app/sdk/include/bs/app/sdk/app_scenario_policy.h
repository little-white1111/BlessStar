#ifndef BS_APP_SDK_APP_SCENARIO_POLICY_H
#define BS_APP_SDK_APP_SCENARIO_POLICY_H

/*
 * C-ST-7 contract block:
 * Thread safety: pure function; caller owns input/output objects.
 * Error semantics: returns false on invalid scenario policy.
 * Platform notes: N/A.
 */

#include <string>
#include <vector>

#include "bs/app/sdk/app_meta_rule.h"

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

    /** Metadata rules for instruction-level policy gates (Day24 MR-02). */
    std::vector<MetaRule> metadata_rules;
};

bool ValidateScenarioPolicy(const ScenarioPolicy& policy);
} // namespace bs::app

#endif // BS_APP_SDK_APP_SCENARIO_POLICY_H
