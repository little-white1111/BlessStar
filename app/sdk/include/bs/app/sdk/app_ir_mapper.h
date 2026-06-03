#ifndef BS_APP_SDK_APP_IR_MAPPER_H
#define BS_APP_SDK_APP_IR_MAPPER_H

/*
 * C-ST-7 contract block:
 * Thread safety: Mapper objects are stack-scoped per request; not shared.
 * Error semantics: MapToIr returns false on schema mismatch; strings hold error detail.
 * Platform notes: Intentional C++17 App surface (not C ABI).
 */

#include <string>

namespace bs::app
{
struct AppConfigModel
{
    std::string source_vendor;
    std::string scenario;
    std::string uri;
    std::string payload;
};

struct IrEnvelope
{
    std::string uri;
    std::string payload;
};

// Minimal shape mapping: App semantics -> generic IR envelope.
bool MapToIr(const AppConfigModel& in, IrEnvelope* out);
} // namespace bs::app

#endif // BS_APP_SDK_APP_IR_MAPPER_H
