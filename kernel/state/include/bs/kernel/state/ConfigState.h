#ifndef BS_KERNEL_STATE_CONFIGSTATE_H
#define BS_KERNEL_STATE_CONFIGSTATE_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ConfigState
    {
        CONFIG_STATE_INITIAL,
        CONFIG_STATE_LOADING,
        CONFIG_STATE_ACTIVE,
        CONFIG_STATE_UPDATING,
        CONFIG_STATE_ERROR,
        CONFIG_STATE_CLOSED
    } ConfigState;

    const char* ConfigState_ToString(ConfigState state);

#ifdef __cplusplus
}
#endif

#endif
