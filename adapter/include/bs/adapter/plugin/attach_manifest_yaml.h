#ifndef BS_ADAPTER_PLUGIN_ATTACH_MANIFEST_YAML_H
#define BS_ADAPTER_PLUGIN_ATTACH_MANIFEST_YAML_H

#ifdef __cplusplus
extern "C"
{
#endif

    struct AttachManifestPluginConfig
    {
        const char* manifest_id;
        int         enabled; /* -1 = use built-in default */
        int         depends_count;
        const char* depends_on[8];
    };

    /**
     * Minimal SCHEMA-ATTACH-VIII-1 parser (enabled / depends_on only).
     * @param out_configs caller array; @param max_configs capacity
     * @return count written, or -1 on parse error
     */
    int bs_adapter_attach_manifest_yaml_load(const char*                 path,
                                             AttachManifestPluginConfig* out_configs,
                                             int                         max_configs);

    /** Free strings allocated by bs_adapter_attach_manifest_yaml_load. */
    void bs_adapter_attach_manifest_yaml_free_configs(AttachManifestPluginConfig* configs,
                                                      int                         count);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PLUGIN_ATTACH_MANIFEST_YAML_H */
