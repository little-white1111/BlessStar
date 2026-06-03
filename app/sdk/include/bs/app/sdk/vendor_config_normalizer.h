#ifndef BS_APP_SDK_VENDOR_CONFIG_NORMALIZER_H
#define BS_APP_SDK_VENDOR_CONFIG_NORMALIZER_H

/*
 * C-ST-7 contract block:
 * Thread safety: not thread-safe; one normalizer call per file path at a time.
 * Error semantics: NormalizeVendorConfig returns false and fills NormalizeResult::error.
 * Platform notes: reads vendor files from disk only (no network).
 */

#include <cstdint>

#include <string>
#include <vector>

namespace bs::app
{

enum class VendorFormat
{
    GenericBusinessJson = 0
    // Yonyou, Kingdee: phase 2 (VP-5)
};

struct NormalizeResult
{
    bool                      ok = false;
    std::vector<std::uint8_t> v1_bytes;
    std::string               source_vendor;
    std::string               scenario;
    std::string               error;
};

/** Read vendor/business file and emit BlessStar Config JSON v1 bytes (VP-4). */
bool NormalizeVendorConfig(VendorFormat fmt, const std::string& vendor_file_path,
                           NormalizeResult* out);

} // namespace bs::app

#endif // BS_APP_SDK_VENDOR_CONFIG_NORMALIZER_H
