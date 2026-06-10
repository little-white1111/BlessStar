#include "bs/kernel/common/bs_wait_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#endif
#include <pthread.h>
#include <time.h>
#endif

enum BsWaitTraceMode
{
    BS_WAIT_TRACE_OFF  = 0,
    BS_WAIT_TRACE_FULL = 1,
    BS_WAIT_TRACE_HANG = 2
};

static int g_wait_trace_mode   = BS_WAIT_TRACE_OFF;
static int g_hang_threshold_ms = 3000;

#ifdef _WIN32
static INIT_ONCE g_wait_trace_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK init_wait_trace_once(PINIT_ONCE once, PVOID param, PVOID* ctx)
{
    (void)once;
    (void)param;
    (void)ctx;
    const char* env = getenv("BS_WAIT_TRACE");
    if (env && env[0] == '1' && env[1] == '\0')
        g_wait_trace_mode = BS_WAIT_TRACE_FULL;
    else if (env && strcmp(env, "hang") == 0)
        g_wait_trace_mode = BS_WAIT_TRACE_HANG;
    env = getenv("BS_WAIT_TRACE_HANG_MS");
    g_hang_threshold_ms = (env && env[0]) ? atoi(env) : 3000;
    if (g_hang_threshold_ms < 100)
        g_hang_threshold_ms = 100;
    return TRUE;
}
#else
static pthread_once_t g_wait_trace_once = PTHREAD_ONCE_INIT;

static void init_wait_trace_once(void)
{
    const char* env = getenv("BS_WAIT_TRACE");
    if (env && env[0] == '1' && env[1] == '\0')
        g_wait_trace_mode = BS_WAIT_TRACE_FULL;
    else if (env && strcmp(env, "hang") == 0)
        g_wait_trace_mode = BS_WAIT_TRACE_HANG;
    env = getenv("BS_WAIT_TRACE_HANG_MS");
    g_hang_threshold_ms = (env && env[0]) ? atoi(env) : 3000;
    if (g_hang_threshold_ms < 100)
        g_hang_threshold_ms = 100;
}
#endif

static int wait_trace_mode(void)
{
#ifdef _WIN32
    (void)InitOnceExecuteOnce(&g_wait_trace_once, init_wait_trace_once, NULL, NULL);
#else
    (void)pthread_once(&g_wait_trace_once, init_wait_trace_once);
#endif
    return g_wait_trace_mode;
}

static int hang_threshold_ms(void)
{
    (void)wait_trace_mode();
    return g_hang_threshold_ms;
}

static unsigned long current_thread_id(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentThreadId();
#else
    return (unsigned long)pthread_self();
#endif
}

static unsigned long long now_ms(void)
{
#ifdef _WIN32
    return (unsigned long long)GetTickCount64();
#else
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000ull + (unsigned long long)ts.tv_nsec / 1000000ull;
#endif
}

static void emit_frames(const char* site, unsigned long long ctx, int has_ctx,
                        unsigned long long waited_ms)
{
    void* frames[24];
    int   count = 0;

#ifdef _WIN32
    count = (int)CaptureStackBackTrace(2, 24, frames, NULL);
#else
#if defined(__linux__) || defined(__APPLE__)
    count = backtrace(frames, 24);
    if (count > 2)
        count -= 2;
    if (count < 0)
        count = 0;
#else
    (void)frames;
#endif
#endif

    if (has_ctx)
    {
        fprintf(stderr, "[BS_WAIT_TRACE] site=%s tid=%lu ctx=%llu waited_ms=%llu frames=%d\n", site,
                current_thread_id(), ctx, waited_ms, count);
    }
    else
    {
        fprintf(stderr, "[BS_WAIT_TRACE] site=%s tid=%lu waited_ms=%llu frames=%d\n", site,
                current_thread_id(), waited_ms, count);
    }

    for (int i = 0; i < count; ++i)
        fprintf(stderr, "  #%02d %p\n", i, frames[i]);
    fflush(stderr);
}

static void emit_if_enabled(const char* site, unsigned long long ctx, int has_ctx,
                            unsigned long long waited_ms)
{
    const int mode = wait_trace_mode();
    if (mode == BS_WAIT_TRACE_OFF)
        return;
    if (mode == BS_WAIT_TRACE_FULL)
    {
        emit_frames(site, ctx, has_ctx, waited_ms);
        return;
    }
    if (waited_ms < (unsigned long long)hang_threshold_ms())
        return;
    emit_frames(site, ctx, has_ctx, waited_ms);
}

void bs_wait_trace(const char* site)
{
    emit_if_enabled(site, 0, 0, 0);
}

void bs_wait_trace_u64(const char* site, unsigned long long ctx)
{
    emit_if_enabled(site, ctx, 1, 0);
}

void bs_wait_trace_path(const char* site, const char* path)
{
    const int mode = wait_trace_mode();
    if (mode == BS_WAIT_TRACE_OFF || !site)
        return;
    fprintf(stderr, "[BS_WAIT_TRACE] site=%s tid=%lu path=%s\n", site, current_thread_id(),
            path && path[0] ? path : "<null>");
    fflush(stderr);
}

int bs_wait_trace_hang_begin(const char* site)
{
    const int mode = wait_trace_mode();
    if (mode == BS_WAIT_TRACE_OFF || !site)
        return -1;
    return (int)now_ms();
}

void bs_wait_trace_hang_tick(const char* site, int token)
{
    if (token < 0 || !site)
        return;
    const unsigned long long waited = now_ms() - (unsigned long long)token;
    emit_if_enabled(site, 0, 0, waited);
}

void bs_wait_trace_hang_tick_u64(const char* site, int token, unsigned long long ctx)
{
    if (token < 0 || !site)
        return;
    const unsigned long long waited = now_ms() - (unsigned long long)token;
    emit_if_enabled(site, ctx, 1, waited);
}

void bs_wait_trace_hang_end(const char* site, int token)
{
    if (token < 0 || !site)
        return;
    const unsigned long long waited = now_ms() - (unsigned long long)token;
    emit_if_enabled(site, 0, 0, waited);
}
