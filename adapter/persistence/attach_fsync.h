#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_FSYNC_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_FSYNC_H

/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; caller serializes FILE* access.
 * Error semantics: Returns 0 on success, non-zero errno-style on flush/fsync failure.
 * Platform notes: Windows uses FlushFileBuffers; POSIX uses fflush+fsync.
 */

#ifdef __cplusplus
extern "C"
{
#endif

    /** Flush and fsync an open FILE* (ATOM-XIV-4). Returns 0 on success. */
    int bs_adapter_attach_persist_fsync_file(void* file_handle);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_FSYNC_H */
