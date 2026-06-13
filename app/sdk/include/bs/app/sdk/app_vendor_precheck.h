#ifndef BS_APP_SDK_APP_VENDOR_PRECHECK_H
#define BS_APP_SDK_APP_VENDOR_PRECHECK_H

/*
 * C-ST-7 contract block:
 * Thread safety: pure on inputs; policy pointer must outlive call.
 * Error semantics: returns false when v1 shape or ScenarioPolicy invalid.
 * Platform notes: does not parse vendor formats; v1 bytes only (T5.4).
 */

#include <cstddef>
#include <cstdint>

#include <string>

#include "bs/kernel/ir/ir.h"

#include "bs/app/sdk/app_scenario_policy.h"

namespace bs::app
{

/** Sample business final-check on v1 bytes + ScenarioPolicy (not vendor-to-v1 conversion). */
bool PrecheckV1BytesForScenario(const std::uint8_t* data, std::size_t len,
                                const ScenarioPolicy& policy, std::string* error_out);

/**
 * Extended precheck: validates v1 bytes and then checks metadata rules
 * against the already-parsed IRInstructionList (MR-03).
 *
 * Instructions are the parsed result from the default gate; this avoids
 * a second parse while enabling structured metadata rule checks.
 */
bool PrecheckV1InstructionsForScenario(
    const std::uint8_t* data, std::size_t len,
    const IRInstructionList* instructions,
    const ScenarioPolicy& policy,
    std::string* error_out);

} // namespace bs::app

#endif // BS_APP_SDK_APP_VENDOR_PRECHECK_H
