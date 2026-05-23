#include "bs/adapter/plugin/plugin_ir_requirements.h"
#include "bs/adapter/plugin/plugin_manifest_paths.h"

#include <cassert>

int main()
{
    char good[512];
    assert(bs_adapter_plugin_manifest_path("ir_requirements_io.txt", good, sizeof(good)) == 0);
    assert(bs_adapter_plugin_validate_ir_requirements_ref(good) == 0);

    char bad[512];
    assert(bs_adapter_plugin_manifest_path("ir_requirements_invalid_fixture.txt", bad,
                                           sizeof(bad)) == 0);
    assert(bs_adapter_plugin_validate_ir_requirements_ref(bad) != 0);

    char empty[512];
    assert(bs_adapter_plugin_manifest_path("ir_requirements_log_domains.txt", empty,
                                           sizeof(empty)) == 0);
    assert(bs_adapter_plugin_validate_ir_requirements_ref(empty) == 0);

    return 0;
}
