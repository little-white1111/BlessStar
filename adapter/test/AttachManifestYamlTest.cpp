#include "bs/adapter/plugin/attach_manifest_yaml.h"
#include "bs/adapter/plugin/plugin_manifest_paths.h"

#include <cassert>
#include <cstring>

int main()
{
    char path[512];
    assert(bs_adapter_plugin_manifest_path("attach_plugins.yaml", path, sizeof(path)) == 0);

    AttachManifestPluginConfig cfgs[8];
    const int                  n = bs_adapter_attach_manifest_yaml_load(path, cfgs, 8);
    assert(n == 3);

    for (int i = 0; i < n; ++i)
    {
        if (std::strcmp(cfgs[i].manifest_id, "orch-reload") == 0)
        {
            assert(cfgs[i].enabled == 1);
            assert(cfgs[i].depends_count >= 2);
        }
    }
    return 0;
}
