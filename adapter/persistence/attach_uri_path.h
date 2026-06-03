#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_URI_PATH_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_URI_PATH_H

/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; pure string transform on caller buffers.
 * Error semantics: Returns BS_ATTACH_OK or BS_ATTACH_ERR_* on invalid URI/path.
 * Platform notes: file:// only for MVP; see attach_store.h for URI rules.
 */

#include "bs/adapter/persistence/attach_store.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int bs_adapter_attach_persist_uri_to_path(const char* uri, char* out_path, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_URI_PATH_H */
