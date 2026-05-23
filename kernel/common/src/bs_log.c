#include "bs/kernel/common/bs_log.h"
#include "bs/kernel/common/bs_safe_format.h"

#include <stdio.h>
#include <string.h>

static BsLogState  g_fallback_log_state;
static int         g_fallback_initialized = 0;
static BsLogState* g_current_log_state    = NULL;

static void ensure_fallback_initialized(void)
{
    if (!g_fallback_initialized)
    {
        bs_log_state_init(&g_fallback_log_state);
        g_fallback_initialized = 1;
    }
}

static BsLogState* resolve_state(BsLogState* explicit_state)
{
    if (explicit_state)
        return explicit_state;
    if (g_current_log_state)
        return g_current_log_state;
    ensure_fallback_initialized();
    return &g_fallback_log_state;
}

void bs_log_state_init(BsLogState* state)
{
    if (!state)
        return;
    state->bus          = NULL;
    state->global_level = BS_LOG_INFO;
    state->ring_head    = 0;
    state->ring_count   = 0;
}

void bs_log_set_current_state(BsLogState* state)
{
    g_current_log_state = state;
}

BsLogState* bs_log_get_current_state(void)
{
    return resolve_state(NULL);
}

static int level_passes_filter(const BsLogState* state, BsLogLevel level)
{
    if (!state)
        return 0;
    if (level == BS_LOG_AUDIT)
        return 1;
    if (level == BS_LOG_OFF)
        return 0;
    return level >= state->global_level;
}

static void ring_push(BsLogState* state, uint16_t domain_id, BsLogLevel level,
                      const char* formatted)
{
    if (!state)
        return;

    BsLogRingEntry* slot = &state->ring[state->ring_head];
    slot->domain_id      = domain_id;
    slot->level          = level;
    bs_safe_snprintf(slot->message, sizeof(slot->message), "%s", formatted ? formatted : "");
    state->ring_head = (state->ring_head + 1) % BS_LOG_RING_CAPACITY;
    if (state->ring_count < BS_LOG_RING_CAPACITY)
        ++state->ring_count;
    else
        state->ring_count = BS_LOG_RING_CAPACITY;
}

static void flush_ring(BsLogState* state)
{
    if (!state || !state->bus || !state->bus->emit)
        return;

    BsLogRingEntry pending[BS_LOG_RING_CAPACITY];
    const unsigned count = state->ring_count;
    const unsigned start = (count < BS_LOG_RING_CAPACITY) ? 0 : state->ring_head;
    for (unsigned i = 0; i < count; ++i)
    {
        const unsigned idx = (start + i) % BS_LOG_RING_CAPACITY;
        pending[i]         = state->ring[idx];
    }
    state->ring_head  = 0;
    state->ring_count = 0;

    for (unsigned i = 0; i < count; ++i)
    {
        const BsLogRingEntry* e = &pending[i];
        if (!level_passes_filter(state, e->level))
            continue;
        bs_log_emit_ctx(state, e->domain_id, e->level, "%s", e->message);
    }
}

int bs_log_bind_bus_ctx(BsLogState* ctx, const BsLogBusOps* ops)
{
    BsLogState* state = resolve_state(ctx);
    if (!state || !ops || !ops->emit)
        return -1;
    state->bus = ops;
    flush_ring(state);
    if (state->bus->flush)
        state->bus->flush();
    return 0;
}

int bs_log_bus_is_bound_ctx(const BsLogState* ctx)
{
    const BsLogState* state = resolve_state((BsLogState*)ctx);
    return state && state->bus != NULL && state->bus->emit != NULL;
}

void bs_log_set_global_level_ctx(BsLogState* ctx, BsLogLevel level)
{
    BsLogState* state = resolve_state(ctx);
    if (!state)
        return;
    if (level >= BS_LOG_OFF)
        state->global_level = BS_LOG_OFF;
    else
        state->global_level = level;
}

BsLogLevel bs_log_get_global_level_ctx(const BsLogState* ctx)
{
    const BsLogState* state = resolve_state((BsLogState*)ctx);
    return state ? state->global_level : BS_LOG_INFO;
}

void bs_log_emit_ctx(BsLogState* ctx, uint16_t domain_id, BsLogLevel level, const char* fmt, ...)
{
    if (!fmt)
        return;

    BsLogState* state = resolve_state(ctx);

    char    formatted[512];
    va_list ap;
    va_start(ap, fmt);
    bs_safe_vsnprintf(formatted, sizeof(formatted), fmt, ap);
    va_end(ap);

    if (!state->bus || !state->bus->emit)
    {
        ring_push(state, domain_id, level, formatted);
        return;
    }

    if (!level_passes_filter(state, level))
        return;

    va_list ap2;
    va_start(ap2, fmt);
    state->bus->emit(domain_id, level, fmt, ap2);
    va_end(ap2);
}

void bs_log_flush_ctx(BsLogState* ctx)
{
    BsLogState* state = resolve_state(ctx);
    if (state && state->bus && state->bus->flush)
        state->bus->flush();
}

int bs_log_bind_bus(const BsLogBusOps* ops)
{
    return bs_log_bind_bus_ctx(NULL, ops);
}

int bs_log_bus_is_bound(void)
{
    return bs_log_bus_is_bound_ctx(NULL);
}

void bs_log_set_global_level(BsLogLevel level)
{
    bs_log_set_global_level_ctx(NULL, level);
}

BsLogLevel bs_log_get_global_level(void)
{
    return bs_log_get_global_level_ctx(NULL);
}

void bs_log_emit(uint16_t domain_id, BsLogLevel level, const char* fmt, ...)
{
    if (!fmt)
        return;

    BsLogState* state = resolve_state(NULL);

    if (!state->bus || !state->bus->emit)
    {
        char    formatted[512];
        va_list ap;
        va_start(ap, fmt);
        bs_safe_vsnprintf(formatted, sizeof(formatted), fmt, ap);
        va_end(ap);
        ring_push(state, domain_id, level, formatted);
        return;
    }

    if (!level_passes_filter(state, level))
        return;

    va_list ap2;
    va_start(ap2, fmt);
    state->bus->emit(domain_id, level, fmt, ap2);
    va_end(ap2);
}

void bs_log_flush(void)
{
    bs_log_flush_ctx(NULL);
}
