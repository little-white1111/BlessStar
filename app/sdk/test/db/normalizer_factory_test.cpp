// DB-E-08 test file 2: Normalizer factory
#include "bs/app/sdk/common/bs_factory.h"
#include "bs/app/sdk/vendor_config_normalizer.h"

#include <cassert>
#include <iostream>

int main()
{
    bs::app::NormalizerFactory factory;
    bool registered = factory.Register(1, [](const std::string&, bs::app::NormalizeResult* out) -> bool {
        out->ok = true;
        return true;
    });
    assert(registered);
    assert(factory.Has(1));
    std::cout << "Normalizer factory test passed.\n";
    return 0;
}
