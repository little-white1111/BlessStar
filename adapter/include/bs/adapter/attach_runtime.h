#ifndef BS_ADAPTER_ATTACH_RUNTIME_H
#define BS_ADAPTER_ATTACH_RUNTIME_H

#ifdef __cplusplus
extern "C"
{
#endif

    /** True after successful bootstrap bind_log_bus (LOG-VII-10). */
    int bs_adapter_attach_is_log_ready(void);

    void bs_adapter_attach_mark_log_ready(int ready);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_RUNTIME_H */
