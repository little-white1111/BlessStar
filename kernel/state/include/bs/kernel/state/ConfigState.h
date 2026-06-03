#ifndef BS_KERNEL_STATE_CONFIGSTATE_H
#define BS_KERNEL_STATE_CONFIGSTATE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Pure enum / string helper; reentrant.
 * Error semantics: N/A (no failing API besides unknown state string).
 * Platform notes: Drives ConfigEventType ENTER_* mapping in ConfigManager.
 */

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

    const char* bs_config_state_to_string(ConfigState state);

#ifdef __cplusplus
}
#endif

#endif
