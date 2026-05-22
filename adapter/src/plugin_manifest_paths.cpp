#include "bs/adapter/plugin/plugin_manifest_paths.h"

#include "bs/adapter/manifest/manifest_config.h"

#include <cstring>

int bs_adapter_plugin_manifest_path(const char* relative_or_absolute, char* out_buf,
                                    size_t out_buf_len)
{
    if (!relative_or_absolute || !out_buf || out_buf_len == 0)
        return -1;

    if (relative_or_absolute[0] == '/' ||
        (relative_or_absolute[0] != '\0' && relative_or_absolute[1] == ':'))
    {
        if (std::strlen(relative_or_absolute) + 1 > out_buf_len)
            return -1;
        std::strncpy(out_buf, relative_or_absolute, out_buf_len - 1);
        out_buf[out_buf_len - 1] = '\0';
        return 0;
    }

    const char* base = BS_ADAPTER_MANIFEST_DIR;
    if (!base || base[0] == '\0')
        return -1;

    const size_t base_len = std::strlen(base);
    const size_t rel_len  = std::strlen(relative_or_absolute);
    const int    need_sep = (base_len > 0 && base[base_len - 1] != '/');
    const size_t total    = base_len + (need_sep ? 1 : 0) + rel_len;
    if (total + 1 > out_buf_len)
        return -1;

    std::strncpy(out_buf, base, out_buf_len - 1);
    out_buf[out_buf_len - 1] = '\0';
    size_t pos = std::strlen(out_buf);
    if (need_sep)
    {
        out_buf[pos++] = '/';
        out_buf[pos]   = '\0';
    }
    std::strncat(out_buf, relative_or_absolute, out_buf_len - pos - 1);
    return 0;
}
