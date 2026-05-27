#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_FSYNC_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_FSYNC_H

#ifdef __cplusplus
extern "C"
{
#endif

    /** Flush and fsync an open FILE* (ATOM-XIV-4). Returns 0 on success. */
    int bs_attach_fsync_file(void* file_handle);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_FSYNC_H */
