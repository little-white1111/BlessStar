#include "bs/app/sdk/app_ir_mapper.h"

namespace bs::app
{
bool MapToIr(const AppConfigModel& in, IrEnvelope* out)
{
    if (!out)
        return false;
    if (in.uri.empty() || in.payload.empty())
        return false;
    if (!in.source_vendor.empty() && in.scenario.empty())
        return false;
    out->uri     = in.uri;
    out->payload = in.payload;
    return true;
}
} // namespace bs::app
