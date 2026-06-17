// DB-E-08 test file 3: Memory-based normalization
#include "bs/app/sdk/vendor_config_normalizer.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main()
{
    // Simple test: raw memory normalization for GenericBusinessJson (stub)
    const char* json = "{\"kernel_version\":\"v1\",\"instructions\":{}}";
    size_t len = strlen(json);
    std::vector<uint8_t> data(reinterpret_cast<const uint8_t*>(json), reinterpret_cast<const uint8_t*>(json) + len);

    bs::app::NormalizeResult out;
    bool ok = bs::app::NormalizeVendorConfig(bs::app::VendorFormat::GenericBusinessJson,
                                             data.data(), data.size(), &out);
    assert(out.ok);
    assert(!out.v1_bytes.empty());
    std::cout << "Memory-based normalization test passed.\n";
    return 0;
}
