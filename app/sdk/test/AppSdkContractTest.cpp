#include <cassert>

#include "bs/app/sdk/app_contract.h"
#include "bs/app/sdk/app_ir_mapper.h"
#include "bs/app/sdk/app_scenario_policy.h"

int main()
{
    using namespace bs::app;

    assert(IsCallAllowed(Layer::App, Layer::Adapter));
    assert(IsCallAllowed(Layer::Adapter, Layer::Kernel));
    assert(!IsCallAllowed(Layer::App, Layer::Kernel)); // Must go through adapter.
    assert(!IsCallAllowed(Layer::Kernel, Layer::Adapter));

    AppConfigModel in;
    in.source_vendor = "generic_business";
    in.scenario      = "expense_reimburse";
    in.uri           = "file:///finance/expense.json";
    in.payload       = "{\"k\":1}";
    IrEnvelope out;
    assert(MapToIr(in, &out));
    assert(out.uri == in.uri);
    assert(out.payload == in.payload);

    AppConfigModel bad;
    bad.uri     = "";
    bad.payload = "{}";
    assert(!MapToIr(bad, &out));

    AppConfigModel bad_vendor;
    bad_vendor.source_vendor = "generic_business";
    bad_vendor.scenario      = "";
    bad_vendor.uri           = "file:///x.json";
    bad_vendor.payload       = "{}";
    assert(!MapToIr(bad_vendor, &out));

    ScenarioPolicy policy;
    policy.type             = ScenarioType::ExpenseReimburse;
    policy.tenant           = "tenant-a";
    policy.allow_hot_reload = true;
    policy.max_batch        = 64;
    assert(ValidateScenarioPolicy(policy));

    ScenarioPolicy bad_policy = policy;
    bad_policy.tenant         = "";
    assert(!ValidateScenarioPolicy(bad_policy));
    return 0;
}
