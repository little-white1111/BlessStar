#include "bs/adapter/persistence/attach_watch.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION g_watch_cs;
static int              g_watch_cs_init = 0;

static void watch_lock_init(void)
{
    if (!g_watch_cs_init)
    {
        InitializeCriticalSection(&g_watch_cs);
        g_watch_cs_init = 1;
    }
}

static void watch_lock(void)
{
    watch_lock_init();
    EnterCriticalSection(&g_watch_cs);
}

static void watch_unlock(void)
{
    LeaveCriticalSection(&g_watch_cs);
}
#else
#include <pthread.h>
static pthread_mutex_t g_watch_mu = PTHREAD_MUTEX_INITIALIZER;

static void watch_lock(void)
{
    pthread_mutex_lock(&g_watch_mu);
}

static void watch_unlock(void)
{
    pthread_mutex_unlock(&g_watch_mu);
}
#endif

#define BS_ATTACH_WATCH_MAX_SUBSCRIBERS 32
#define BS_ATTACH_WATCH_DEDUPE_SLOTS 1024 /* P1: fixed dedupe capacity contract. */

typedef struct WatchSlot
{
    int                     used;
    int                     token;
    BsAttachWatchSubscriber fn;
    void*                   user;
} WatchSlot;

static WatchSlot g_slots[BS_ATTACH_WATCH_MAX_SUBSCRIBERS];
static int       g_next_token = 1;

typedef struct DedupeEntry
{
    uint64_t epoch;
    uint32_t stage;
    uint32_t uri_hash;
    int      valid;
} DedupeEntry;

static DedupeEntry g_dedupe[BS_ATTACH_WATCH_DEDUPE_SLOTS];
static size_t      g_dedupe_cursor = 0;

static BsAttachWatchMetrics g_metrics;
static BsAttachWatchAudit   g_audit;

static uint32_t hash_uri(const char* s)
{
    if (!s)
        return 0;
    uint32_t h = 2166136261u;
    while (*s)
    {
        h ^= (uint8_t)(*s++);
        h *= 16777619u;
    }
    return h;
}

static int dedupe_accept(const BsAttachWatchEvent* ev)
{
    if (!ev)
        return 0;
    const uint32_t h = hash_uri(ev->uri);
    for (size_t i = 0; i < BS_ATTACH_WATCH_DEDUPE_SLOTS; ++i)
    {
        if (g_dedupe[i].valid && g_dedupe[i].epoch == ev->epoch &&
            g_dedupe[i].stage == (uint32_t)ev->stage && g_dedupe[i].uri_hash == h)
        {
            g_metrics.dropped_duplicates++;
            return 0;
        }
    }
    g_dedupe[g_dedupe_cursor].valid    = 1;
    g_dedupe[g_dedupe_cursor].epoch    = ev->epoch;
    g_dedupe[g_dedupe_cursor].stage    = (uint32_t)ev->stage;
    g_dedupe[g_dedupe_cursor].uri_hash = h;
    g_dedupe_cursor                    = (g_dedupe_cursor + 1) % BS_ATTACH_WATCH_DEDUPE_SLOTS;
    return 1;
}

int bs_adapter_attach_persist_watch_subscribe(BsAttachWatchSubscriber fn, void* user,
                                              int* token_out)
{
    if (!fn || !token_out)
        return -1;
    watch_lock();
    for (size_t i = 0; i < BS_ATTACH_WATCH_MAX_SUBSCRIBERS; ++i)
    {
        if (!g_slots[i].used)
        {
            g_slots[i].used  = 1;
            g_slots[i].token = g_next_token++;
            g_slots[i].fn    = fn;
            g_slots[i].user  = user;
            *token_out       = g_slots[i].token;
            watch_unlock();
            return 0;
        }
    }
    watch_unlock();
    return -1;
}

void bs_adapter_attach_persist_watch_unsubscribe(int token)
{
    watch_lock();
    for (size_t i = 0; i < BS_ATTACH_WATCH_MAX_SUBSCRIBERS; ++i)
    {
        if (g_slots[i].used && g_slots[i].token == token)
        {
            memset(&g_slots[i], 0, sizeof(g_slots[i]));
            watch_unlock();
            return;
        }
    }
    watch_unlock();
}

static void record_publish_callback_error(const BsAttachWatchEvent* ev)
{
    if (!ev)
        return;
    g_audit.publish_callback_error_count++;
    g_audit.last_callback_error_epoch = ev->epoch;
    g_audit.last_callback_error_stage = (uint32_t)ev->stage;
    if ((uint32_t)ev->stage <
        (sizeof(g_audit.callback_error_by_stage) / sizeof(g_audit.callback_error_by_stage[0])))
        g_audit.callback_error_by_stage[(uint32_t)ev->stage]++;
}

int bs_adapter_attach_persist_watch_publish(const BsAttachWatchEvent* ev)
{
    if (!ev)
        return -1;

    BsAttachWatchSubscriber fns[BS_ATTACH_WATCH_MAX_SUBSCRIBERS];
    void*                   users[BS_ATTACH_WATCH_MAX_SUBSCRIBERS];
    size_t                  n = 0;

    watch_lock();
    for (size_t i = 0; i < BS_ATTACH_WATCH_MAX_SUBSCRIBERS; ++i)
    {
        if (!g_slots[i].used || !g_slots[i].fn)
            continue;
        fns[n]   = g_slots[i].fn;
        users[n] = g_slots[i].user;
        ++n;
    }
    watch_unlock();

    int rc = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const int sub_rc = fns[i](ev, users[i]);
        if (sub_rc != 0)
        {
            watch_lock();
            record_publish_callback_error(ev);
            watch_unlock();
            rc = sub_rc;
        }
    }
    return rc;
}

void bs_adapter_attach_persist_watch_metrics_reset(void)
{
    watch_lock();
    memset(&g_metrics, 0, sizeof(g_metrics));
    g_metrics.dedupe_capacity = BS_ATTACH_WATCH_DEDUPE_SLOTS;
    memset(g_dedupe, 0, sizeof(g_dedupe));
    g_dedupe_cursor = 0;
    watch_unlock();
}

int bs_adapter_attach_persist_watch_metrics_on_event(const BsAttachWatchEvent* ev, void* user)
{
    (void)user;
    if (!ev)
        return -1;
    watch_lock();
    if (!dedupe_accept(ev))
    {
        watch_unlock();
        return 0;
    }
    g_metrics.total_events++;
    const uint32_t stage = (uint32_t)ev->stage;
    if (stage < (sizeof(g_metrics.stage_counts) / sizeof(g_metrics.stage_counts[0])))
        g_metrics.stage_counts[stage]++;
    if (ev->result == BS_ATTACH_WATCH_RESULT_FAIL)
        g_metrics.fail_count++;
    watch_unlock();
    return 0;
}

void bs_adapter_attach_persist_watch_metrics_snapshot(BsAttachWatchMetrics* out)
{
    if (!out)
        return;
    watch_lock();
    *out = g_metrics;
    watch_unlock();
}

void bs_adapter_attach_persist_watch_audit_reset(void)
{
    watch_lock();
    memset(&g_audit, 0, sizeof(g_audit));
    watch_unlock();
}

int bs_adapter_attach_persist_watch_audit_on_event(const BsAttachWatchEvent* ev, void* user)
{
    (void)user;
    if (!ev)
        return -1;
    watch_lock();
    if (ev->stage == BS_ATTACH_WATCH_STAGE_RECOVER_CONSERVATIVE)
        g_audit.conservative_recover_count++;
    if (ev->result == BS_ATTACH_WATCH_RESULT_FAIL)
        g_audit.publish_fail_count++;
    watch_unlock();
    return 0;
}

void bs_adapter_attach_persist_watch_audit_snapshot(BsAttachWatchAudit* out)
{
    if (!out)
        return;
    watch_lock();
    *out = g_audit;
    watch_unlock();
}

size_t bs_adapter_attach_persist_watch_dedupe_capacity(void)
{
    return (size_t)BS_ATTACH_WATCH_DEDUPE_SLOTS;
}
