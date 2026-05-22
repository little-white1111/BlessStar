#ifndef BS_ADAPTER_PLUGIN_PLUGIN_MANIFEST_PATHS_H
#define BS_ADAPTER_PLUGIN_PLUGIN_MANIFEST_PATHS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Join BS_ADAPTER_MANIFEST_DIR (when defined) with @p relative_name into @p out_buf.
     * If @p relative_or_absolute is already absolute, copies as-is.
     * @return 0 ok, non-zero if buffer too small or path unavailable
     */
    int bs_adapter_plugin_manifest_path(const char* relative_or_absolute, char* out_buf,
                                        size_t out_buf_len);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PLUGIN_PLUGIN_MANIFEST_PATHS_H */
