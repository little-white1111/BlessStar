#include "bs/app/sdk/db/db_config.h"
#include "bs/app/sdk/db/db_factory.h"
#include "bs/app/sdk/vendor_config_normalizer.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main()
{
    // 1. Test DatabaseConfig initialization
    bs::app::DatabaseConfig cfg;
    assert(cfg.driver_type == bs::app::DbDriverType::Null);
    assert(cfg.pool_size == 4);
    assert(cfg.timeout_ms == 5000);

    // 2. Test factory registration
    bs::app::DbDriverFactory& factory = bs::app::DbDriverFactory::Instance();
    bool registered = factory.Register(bs::app::DbDriverType::Null,
        [](const bs::app::DatabaseConfig&) -> bs::app::DbConnector* { return nullptr; });
    assert(registered);
    assert(factory.Has(bs::app::DbDriverType::Null));

    // 3. Test NormalizeVendorConfig with raw bytes (if needed)
    // Not testing full parsing here; just linking the stub
    std::cout << "DB configuration test passed.\n";
    return 0;
}
