#include "bs/kernel/registry/path_normalize.h"

#include <cstring>

int bs_registry_normalize_path(const char* in, char* out, size_t out_size)
{
    if (!in || !out || out_size < 2)
        return BS_REGISTRY_ERR_INVALID_ARG;

    if (in[0] != '/')
        return BS_REGISTRY_ERR_INVALID_PATH;

    size_t w = 0;
    out[w++] = '/';

    const char* p = in + 1;
    while (*p)
    {
        while (*p == '/')
            ++p;
        if (!*p)
            break;

        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0'))
            return BS_REGISTRY_ERR_INVALID_PATH;
        if (p[0] == '.' && (p[1] == '/' || p[1] == '\0'))
        {
            p += (p[1] == '/') ? 2 : 1;
            continue;
        }

        const char* seg_start = p;
        while (*p && *p != '/')
            ++p;
        const size_t seg_len = static_cast<size_t>(p - seg_start);
        if (seg_len == 0)
            continue;
        if (w + seg_len + 1 >= out_size)
            return BS_REGISTRY_ERR_INVALID_PATH;
        if (w > 1)
            out[w++] = '/';
        std::memcpy(out + w, seg_start, seg_len);
        w += seg_len;
    }

    if (w == 1)
    {
        out[1] = '\0';
        return BS_REGISTRY_OK;
    }
    out[w] = '\0';
    return BS_REGISTRY_OK;
}

int bs_registry_path_has_allowed_root(const char* normalized_path)
{
    if (!normalized_path || normalized_path[0] != '/')
        return 0;
    if (std::strncmp(normalized_path, "/kernel", 7) == 0 &&
        (normalized_path[7] == '\0' || normalized_path[7] == '/'))
        return 1;
    if (std::strncmp(normalized_path, "/adapter", 8) == 0 &&
        (normalized_path[8] == '\0' || normalized_path[8] == '/'))
        return 1;
    return 0;
}
