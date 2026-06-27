/* ── bs_trace_span.c ─────────────────────────────────────────────────
 * Tracing span context implementation.
 * In-memory span ring buffer + optional OTLP export.
 * DAY38-11: 8 instrumented sites + span context
 * ──────────────────────────────────────────────────────────────────── */

#include "bs/kernel/common/bs_trace_span.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* ── Constants ────────────────────────────────────────────────────── */
#define BS_TRACE_MAX_SPANS   1024
#define BS_TRACE_MAX_ATTRS   16
#define BS_TRACE_MAX_EVENTS  64

/* ── Span struct ──────────────────────────────────────────────────── */
struct bs_trace_span {
    bs_trace_id_t trace_id;
    bs_span_id_t  span_id;
    bs_span_id_t  parent_span_id;
    char          name[128];
    int           sampled;

    bs_trace_attr_t attrs[BS_TRACE_MAX_ATTRS];
    size_t         attr_count;

    char           events[BS_TRACE_MAX_EVENTS][64];
    size_t         event_count;

    int            has_error;
    char           error_message[256];

    /* timing */
    uint64_t       start_ns;
    uint64_t       end_ns;
    int            finished;
};

/* ── Global span ring buffer ──────────────────────────────────────── */
static bs_trace_span_t g_spans[BS_TRACE_MAX_SPANS];
static size_t          g_span_count = 0;
static size_t          g_span_next  = 0;

/* OTLP endpoint */
static char g_otlp_endpoint[512] = {0};

/* ── ID generation ────────────────────────────────────────────────── */
static uint64_t trace_id_counter = 0;

static uint64_t now_ns(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (uint64_t)((double)cnt.QuadPart / (double)freq.QuadPart * 1e9);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

bs_trace_id_t bs_trace_generate_id(void)
{
    /* simple incrementing ID; for production use random + host */
    return ++trace_id_counter;
}

/* ── Span lifecycle ───────────────────────────────────────────────── */
bs_trace_span_t* bs_trace_span_start(
    bs_trace_id_t        trace_id,
    bs_span_id_t         parent_span_id,
    const char*          name,
    const bs_trace_attr_t* attrs,
    size_t               attr_count)
{
    if (g_span_next >= BS_TRACE_MAX_SPANS) {
        g_span_next = 0; /* wrap */
    }

    bs_trace_span_t* s = &g_spans[g_span_next];
    memset(s, 0, sizeof(*s));

    s->trace_id       = trace_id;
    s->span_id        = g_span_next;  /* simple sequential span IDs */
    s->parent_span_id = parent_span_id;
    s->sampled        = 1;
    s->finished       = 0;
    s->start_ns       = now_ns();

    if (name) {
        strncpy(s->name, name, sizeof(s->name) - 1);
    }

    if (attrs) {
        size_t n = (attr_count < BS_TRACE_MAX_ATTRS) ? attr_count : BS_TRACE_MAX_ATTRS;
        for (size_t i = 0; i < n; i++) {
            s->attrs[i].key   = attrs[i].key;   /* borrowed pointer */
            s->attrs[i].value = attrs[i].value;
        }
        s->attr_count = n;
    }

    g_span_next++;
    if (g_span_count < BS_TRACE_MAX_SPANS) g_span_count++;

    return s;
}

void bs_trace_span_finish(bs_trace_span_t* span)
{
    if (!span || span->finished) return;
    span->end_ns   = now_ns();
    span->finished = 1;
}

void bs_trace_span_add_event(bs_trace_span_t* span, const char* event_name)
{
    if (!span || !event_name || span->event_count >= BS_TRACE_MAX_EVENTS) return;
    strncpy(span->events[span->event_count], event_name,
            sizeof(span->events[0]) - 1);
    span->event_count++;
}

void bs_trace_span_add_attr(bs_trace_span_t* span, const char* key, const char* value)
{
    if (!span || !key || span->attr_count >= BS_TRACE_MAX_ATTRS) return;
    span->attrs[span->attr_count].key   = key;
    span->attrs[span->attr_count].value = value;
    span->attr_count++;
}

void bs_trace_span_set_error(bs_trace_span_t* span, const char* message)
{
    if (!span || !message) return;
    span->has_error = 1;
    strncpy(span->error_message, message, sizeof(span->error_message) - 1);
}

/* ── JSON export ──────────────────────────────────────────────────── */
size_t bs_trace_export_json(char** out_json)
{
    if (!out_json) return 0;

    /* estimate capacity: ~512 bytes per span */
    size_t cap = g_span_count * 512 + 128;
    char* buf  = (char*)malloc(cap);
    if (!buf) return 0;

    char* p   = buf;
    char* end = buf + cap;
    int written;

    written = snprintf(p, (size_t)(end - p), "{\"spans\":[");
    if (written < 0) { free(buf); return 0; }
    p += written;

    int first = 1;
    for (size_t i = 0; i < g_span_count; i++) {
        bs_trace_span_t* s = &g_spans[i];
        if (!s->finished) continue;

        if (!first) {
            written = snprintf(p, (size_t)(end - p), ",");
            if (written < 0) continue;
            p += written;
        }
        first = 0;

        uint64_t dur_ns = s->end_ns - s->start_ns;
        written = snprintf(p, (size_t)(end - p),
            "{\"traceId\":\"%llx\",\"spanId\":\"%llx\",\"parentSpanId\":\"%llx\","
            "\"name\":\"%s\",\"startTimeUnixNano\":%llu,\"endTimeUnixNano\":%llu,"
            "\"status\":{\"code\":%d}",
            (unsigned long long)s->trace_id,
            (unsigned long long)s->span_id,
            (unsigned long long)s->parent_span_id,
            s->name,
            (unsigned long long)s->start_ns,
            (unsigned long long)s->end_ns,
            s->has_error ? 2 : 1);
        if (written < 0) continue;
        p += written;

        /* attributes */
        if (s->attr_count > 0) {
            written = snprintf(p, (size_t)(end - p), ",\"attributes\":[");
            if (written < 0) continue;
            p += written;
            for (size_t j = 0; j < s->attr_count; j++) {
                written = snprintf(p, (size_t)(end - p), "%s{\"key\":\"%s\",\"value\":{\"stringValue\":\"%s\"}}",
                    j > 0 ? "," : "",
                    s->attrs[j].key ? s->attrs[j].key : "",
                    s->attrs[j].value ? s->attrs[j].value : "");
                if (written < 0) continue;
                p += written;
            }
            written = snprintf(p, (size_t)(end - p), "]");
            if (written < 0) continue;
            p += written;
        }

        /* error */
        if (s->has_error) {
            written = snprintf(p, (size_t)(end - p),
                ",\"status\":{\"code\":2,\"message\":\"%s\"}", s->error_message);
            if (written < 0) continue;
            p += written;
        }

        written = snprintf(p, (size_t)(end - p), "}");
        if (written < 0) continue;
        p += written;

        if ((unsigned long)(p - buf) > cap - 512) break;
    }

    snprintf(p, (size_t)(end - p), "]}");

    *out_json = buf;
    return strlen(buf);
}

void bs_trace_export_free(char* json)
{
    free(json);
}

/* ── Instrumented site helpers ────────────────────────────────────── */
static const char* site_names[BS_TRACE_SITE_COUNT] = {
    "gate_eval",
    "schema_validator",
    "wal_write",
    "wal_recover",
    "ipc_dispatch",
    "config_read",
    "config_write",
    "plugin_reload",
};

const char* bs_trace_site_name(bs_trace_site_t site)
{
    if (site >= BS_TRACE_SITE_COUNT) return "unknown";
    return site_names[site];
}

bs_trace_span_t* bs_trace_site_span_start(
    bs_trace_site_t        site,
    const bs_trace_attr_t* extra_attrs,
    size_t                 extra_count)
{
    bs_trace_id_t tid = bs_trace_generate_id();
    return bs_trace_span_start(tid, 0, site_names[site], extra_attrs, extra_count);
}

/* ── OTLP (placeholder) ───────────────────────────────────────────── */
void bs_trace_otlp_set_endpoint(const char* endpoint_url)
{
    if (endpoint_url && endpoint_url[0]) {
        strncpy(g_otlp_endpoint, endpoint_url, sizeof(g_otlp_endpoint) - 1);
    } else {
        g_otlp_endpoint[0] = '\0';
    }
}
