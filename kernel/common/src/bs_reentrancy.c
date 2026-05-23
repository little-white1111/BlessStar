#include "bs/kernel/common/bs_reentrancy.h"

#ifdef _WIN32
#include <windows.h>
static __declspec(thread) int g_in_state_callback = 0;
#else
static __thread int g_in_state_callback = 0;
#endif

int bs_reentrancy_in_state_callback(void)
{
    return g_in_state_callback != 0;
}

void bs_reentrancy_enter_state_callback(void)
{
    ++g_in_state_callback;
}

void bs_reentrancy_leave_state_callback(void)
{
    if (g_in_state_callback > 0)
        --g_in_state_callback;
}
