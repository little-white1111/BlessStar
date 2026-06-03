#include "bs/app/sdk/app_contract.h"

namespace bs::app
{
bool IsCallAllowed(Layer caller, Layer callee)
{
    if (caller == Layer::App && callee == Layer::Adapter)
        return true;
    if (caller == Layer::Adapter && callee == Layer::Kernel)
        return true;
    return false;
}
} // namespace bs::app
