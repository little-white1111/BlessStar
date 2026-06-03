#include "bs/adapter/persistence/attach_watch.h"

#include <string.h>

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
    for (size_t i = 0; i < BS_ATTACH_WATCH_MAX_SUBSCRIBERS; ++i)
    {
        if (!g_slots[i].used)
        {
            g_slots[i].used  = 1;
            g_slots[i].token = g_next_token++;
            g_slots[i].fn    = fn;
            g_slots[i].user  = user;
            *token_out       = g_slots[i].token;
            return 0;
        }
    }
    return -1;
}

void bs_adapter_attach_persist_watch_unsubscribe(int token)
{
    for (size_t i = 0; i < BS_ATTACH_WATCH_MAX_SUBSCRIBERS; ++i)
    {
        if (g_slots[i].used && g_slots[i].token == token)
        {
            memset(&g_slots[i], 0, sizeof(g_slots[i]));
            return;
        }
    }
}

int bs_adapter_attach_persist_watch_publish(const BsAttachWatchEvent* ev)
{
    if (!ev)
        return -1;
    int rc = 0;
    for (size_t i = 0; i < BS_ATTACH_WATCH_MAX_SUBSCRIBERS; ++i)
    {
        if (!g_slots[i].used || !g_slots[i].fn)
            continue;
        const int sub_rc = g_slots[i].fn(ev, g_slots[i].user);
        if (sub_rc != 0)
        {
            g_audit.publish_callback_error_count++;
            g_audit.last_callback_error_epoch = ev->epoch;
            g_audit.last_callback_error_stage = (uint32_t)ev->stage;
            if ((uint32_t)ev->stage < (sizeof(g_audit.callback_error_by_stage) /
                                       sizeof(g_audit.callback_error_by_stage[0])))
                g_audit.callback_error_by_stage[(uint32_t)ev->stage]++;
            rc = sub_rc;
        }
    }
    return rc;
}

void bs_adapter_attach_persist_watch_metrics_reset(void)
{
    memset(&g_metrics, 0, sizeof(g_metrics));
    g_metrics.dedupe_capacity = BS_ATTACH_WATCH_DEDUPE_SLOTS;
    memset(g_dedupe, 0, sizeof(g_dedupe));
    g_dedupe_cursor = 0;
}

int bs_adapter_attach_persist_watch_metrics_on_event(const BsAttachWatchEvent* ev, void* user)
{
    (void)user;
    if (!ev)
        return -1;
    if (!dedupe_accept(ev))
        return 0;
    g_metrics.total_events++;
    const uint32_t stage = (uint32_t)ev->stage;
    if (stage < (sizeof(g_metrics.stage_counts) / sizeof(g_metrics.stage_counts[0])))
        g_metrics.stage_counts[stage]++;
    if (ev->result == BS_ATTACH_WATCH_RESULT_FAIL)
        g_metrics.fail_count++;
    return 0;
}

void bs_adapter_attach_persist_watch_metrics_snapshot(BsAttachWatchMetrics* out)
{
    if (!out)
        return;
    *out = g_metrics;
}

void bs_adapter_attach_persist_watch_audit_reset(void)
{
    memset(&g_audit, 0, sizeof(g_audit));
}

int bs_adapter_attach_persist_watch_audit_on_event(const BsAttachWatchEvent* ev, void* user)
{
    (void)user;
    if (!ev)
        return -1;
    if (ev->stage == BS_ATTACH_WATCH_STAGE_RECOVER_CONSERVATIVE)
        g_audit.conservative_recover_count++;
    if (ev->result == BS_ATTACH_WATCH_RESULT_FAIL)
        g_audit.publish_fail_count++;
    return 0;
}

void bs_adapter_attach_persist_watch_audit_snapshot(BsAttachWatchAudit* out)
{
    if (!out)
        return;
    *out = g_audit;
}

size_t bs_adapter_attach_persist_watch_dedupe_capacity(void)
{
    return (size_t)BS_ATTACH_WATCH_DEDUPE_SLOTS;
}
