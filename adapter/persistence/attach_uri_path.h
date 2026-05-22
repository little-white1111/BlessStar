#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_URI_PATH_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_URI_PATH_H

#include "bs/adapter/persistence/attach_store.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int bs_attach_uri_to_path(const char* uri, char* out_path, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif
