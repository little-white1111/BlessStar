#ifndef BS_APP_SDK_APP_CONTRACT_H
#define BS_APP_SDK_APP_CONTRACT_H

/*
 * C-ST-7 contract block:
 * Thread safety: Stateless helpers; reentrant when caller buffers do not alias.
 * Error semantics: IsCallAllowed returns false on disallowed layer hops.
 * Platform notes: App SDK is intentional C++17 surface (not C ABI); see ADR-BS-ABI-001 App
 * exception.
 */

namespace bs::app
{
enum class Layer
{
    App     = 0,
    Adapter = 1,
    Kernel  = 2
};

// Day16 contract baseline: App -> Adapter -> Kernel only.
bool IsCallAllowed(Layer caller, Layer callee);
} // namespace bs::app

#endif // BS_APP_SDK_APP_CONTRACT_H
