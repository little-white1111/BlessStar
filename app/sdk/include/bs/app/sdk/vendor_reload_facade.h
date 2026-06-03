#ifndef BS_APP_SDK_VENDOR_RELOAD_FACADE_H
#define BS_APP_SDK_VENDOR_RELOAD_FACADE_H

/*
 * C-ST-7 contract block:
 * Thread safety: not thread-safe.
 * Error semantics: returns false on normalize/write failure.
 * Platform notes: temp files use host-provided directory (VP-4 host assembly).
 */

#include <string>

#include "bs/app/sdk/vendor_config_normalizer.h"

namespace bs::app
{

/** vendor path -> in-memory v1 bytes (host may write file:// or inject read_fn). */
bool NormalizeFileToV1Bytes(VendorFormat fmt, const std::string& vendor_file_path,
                            NormalizeResult* out);

/**
 * vendor path -> write v1 under temp_dir -> file:/// URI in uri_out.
 * VP-4: App normalize -> file:// -> adapter IO read -> bs_adapter_parser_parse_bytes.
 */
bool NormalizeFileToTempUri(VendorFormat fmt, const std::string& vendor_file_path,
                            const std::string& temp_dir, std::string* uri_out,
                            NormalizeResult* out);

} // namespace bs::app

#endif // BS_APP_SDK_VENDOR_RELOAD_FACADE_H
