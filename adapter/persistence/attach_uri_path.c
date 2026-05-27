#include <string.h>

#include "attach_uri_path.h"

int bs_attach_uri_to_path(const char* uri, char* out_path, size_t out_cap)
{
    if (!uri || !out_path || out_cap == 0)
        return BS_ATTACH_ERR_INVALID_ARG;
    if (strncmp(uri, "file://", 7) != 0)
        return BS_ATTACH_ERR_INVALID_ARG;

    const char* rest = uri + 7;
    if (rest[0] == '/')
    {
        if (rest[1] == '/')
        {
            if (rest[2] == '/')
                rest += 2;
            else if (rest[2] == ':' &&
                     ((rest[1] >= 'A' && rest[1] <= 'Z') ||
                      (rest[1] >= 'a' && rest[1] <= 'z')))
                rest += 1;
            else
                rest += 1;
        }
        else if (rest[2] == ':' &&
                 ((rest[1] >= 'A' && rest[1] <= 'Z') || (rest[1] >= 'a' && rest[1] <= 'z')))
            rest += 1;
        else
            ++rest;
    }
    if (rest[0] == '\0')
        return BS_ATTACH_ERR_INVALID_ARG;

    const size_t n = strlen(rest);
    if (n + 1 > out_cap)
        return BS_ATTACH_ERR_INVALID_ARG;
    memcpy(out_path, rest, n + 1);
    return BS_ATTACH_OK;
}
