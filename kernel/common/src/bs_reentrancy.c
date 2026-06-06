#include "bs/kernel/common/bs_reentrancy.h"

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
static __declspec(thread) int g_in_state_callback      = 0;
static __declspec(thread) int g_in_attach_write        = 0;
static __declspec(thread) int g_in_attach_write_window = 0;
static __declspec(thread) int g_kernel_execute_depth   = 0;
#else
static __thread int g_in_state_callback      = 0;
static __thread int g_in_attach_write        = 0;
static __thread int g_in_attach_write_window = 0;
static __thread int g_kernel_execute_depth   = 0;
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

int bs_reentrancy_in_attach_write(void)
{
    return g_in_attach_write != 0;
}

void bs_reentrancy_enter_attach_write(void)
{
    ++g_in_attach_write;
}

void bs_reentrancy_leave_attach_write(void)
{
    if (g_in_attach_write > 0)
        --g_in_attach_write;
}

int bs_reentrancy_in_attach_write_window(void)
{
    return g_in_attach_write_window != 0;
}

void bs_reentrancy_enter_attach_write_window(void)
{
    ++g_in_attach_write_window;
}

void bs_reentrancy_leave_attach_write_window(void)
{
    if (g_in_attach_write_window > 0)
        --g_in_attach_write_window;
}

int bs_reentrancy_kernel_execute_depth(void)
{
    return g_kernel_execute_depth;
}

void bs_reentrancy_enter_kernel_execute(void)
{
    ++g_kernel_execute_depth;
}

void bs_reentrancy_leave_kernel_execute(void)
{
    if (g_kernel_execute_depth > 0)
        --g_kernel_execute_depth;
}

void bs_reentrancy_trap_listener_write_violation(void)
{
#ifndef NDEBUG
    abort();
#endif
    (void)0;
}
