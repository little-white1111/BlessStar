#include <string>
#include <string_view>
#include <vector>

#include "bs/adapter/parser/meta_rule.h"
#include "bs/app/sdk/app_meta_rule.h"
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

bool PrecheckV1InstructionsForScenario(
    const std::uint8_t* data, std::size_t len,
    const IRInstructionList* instructions,
    const ScenarioPolicy& policy,
    std::string* error_out)
{
    /* First, run the standard v1 byte-level checks. */
    if (!PrecheckV1BytesForScenario(data, len, policy, error_out))
        return false;

    /* No metadata rules -> nothing to check. */
    if (policy.metadata_rules.empty())
        return true;

    /* Convert MetaRule vector to temporary BsMetaRule array using to_c_rule(). */
    const size_t rule_count = policy.metadata_rules.size();
    std::vector<BsMetaRule> c_rules(rule_count);
    std::vector<std::string> instr_names, keys, values;
    instr_names.reserve(rule_count);
    keys.reserve(rule_count);
    values.reserve(rule_count);

    for (size_t i = 0; i < rule_count; ++i)
    {
        const MetaRule& src = policy.metadata_rules[i];
        instr_names.push_back(src.instr_name);
        keys.push_back(src.key);
        values.push_back(src.value);

        to_c_rule(src, &c_rules[i]);
        /* Override pointers to point at copies we own (to_c_rule uses src's internal storage
         * which may be transient, so we redirect to the copies). */
        c_rules[i].instr_name = instr_names.back().c_str();
        c_rules[i].key        = keys.back().c_str();
        if (src.op != BS_META_EXISTS && src.op != BS_META_NOT_EXISTS)
            c_rules[i].value  = values.back().c_str();
        else
            c_rules[i].value  = NULL;
    }

    /* Run the rule engine. */
    char err_buf[256] = {};
    size_t fail_idx = bs_meta_rule_check(instructions, c_rules.data(), rule_count,
                                          err_buf, sizeof(err_buf));

    if (fail_idx != (size_t)-1)
    {
        if (error_out)
        {
            *error_out = "metadata rule #";
            *error_out += std::to_string(fail_idx);
            *error_out += " failed: ";
            *error_out += err_buf;
        }
        return false;
    }

    return true;
}

} // namespace bs::app
