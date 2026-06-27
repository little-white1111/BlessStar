/* ── bs_trace_span.h ──────────────────────────────────────────────────
 * Tracing span context + OpenTelemetry-compatible attributes.
 * DAY38-11: span context (trace_id/span_id) + 8 instrumented sites
 * ──────────────────────────────────────────────────────────────────── */

#ifndef BS_TRACE_SPAN_H
#define BS_TRACE_SPAN_H

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
extern "C" {
#else
#include <stdint.h>
#include <stddef.h>
#endif

/* ── Span context (OTEL-compatible) ──────────────────────────────── */
typedef uint64_t bs_trace_id_t;
typedef uint64_t bs_span_id_t;

typedef struct bs_trace_context {
    bs_trace_id_t trace_id;
    bs_span_id_t  span_id;
    bs_span_id_t  parent_span_id;
    int           sampled;   /* 0 = drop, 1 = keep */
} bs_trace_context_t;

/* ── Span attribute (key-value pair) ─────────────────────────────── */
typedef struct bs_trace_attr {
    const char* key;
    const char* value;
} bs_trace_attr_t;

/* ── Span ────────────────────────────────────────────────────────── */
typedef struct bs_trace_span bs_trace_span_t;

/* ── Lifecycle ───────────────────────────────────────────────────── */
bs_trace_span_t* bs_trace_span_start(
    bs_trace_id_t       trace_id,
    bs_span_id_t        parent_span_id,
    const char*         name,
    const bs_trace_attr_t* attrs,
    size_t              attr_count
);

void bs_trace_span_finish(bs_trace_span_t* span);

/* ── Add event / attribute to active span ────────────────────────── */
void bs_trace_span_add_event(bs_trace_span_t* span, const char* event_name);
void bs_trace_span_add_attr(bs_trace_span_t* span, const char* key, const char* value);

/* ── Status ──────────────────────────────────────────────────────── */
void bs_trace_span_set_error(bs_trace_span_t* span, const char* message);

/* ── Export all finished spans as JSON string ────────────────────── */
/* Caller must free via bs_trace_export_free(). ────────────────────── */
size_t bs_trace_export_json(char** out_json);
void   bs_trace_export_free(char* json);

/* ── 8 instrumented sites ───────────────────────────────────────────
 * These are thin convenience wrappers that delegate to
 * bs_trace_span_start / bs_trace_span_finish for the named sites.
 * ────────────────────────────────────────────────────────────────── */

typedef enum bs_trace_site {
    BS_TRACE_SITE_GATE_EVAL     = 0,  /* gate evaluator evaluation */
    BS_TRACE_SITE_SCHEMA_VAL    = 1,  /* schema validation */
    BS_TRACE_SITE_WAL_WRITE     = 2,  /* WAL batch write */
    BS_TRACE_SITE_WAL_RECOVER   = 3,  /* WAL crash recovery */
    BS_TRACE_SITE_IPC_DISPATCH  = 4,  /* IPC dispatch (executeTool) */
    BS_TRACE_SITE_CONFIG_READ   = 5,  /* config read via bs_config_read */
    BS_TRACE_SITE_CONFIG_WRITE  = 6,  /* config write via bs_config_write */
    BS_TRACE_SITE_PLUGIN_RELOAD = 7,  /* plugin hot-reload */
    BS_TRACE_SITE_COUNT
} bs_trace_site_t;

/* ── Start a span for a named instrumented site ──────────────────── */
bs_trace_span_t* bs_trace_site_span_start(
    bs_trace_site_t site,
    const bs_trace_attr_t* extra_attrs,
    size_t extra_count
);

/* ── Get site name ───────────────────────────────────────────────── */
const char* bs_trace_site_name(bs_trace_site_t site);

/* ── Enable OTLP export (optional; off by default) ───────────────── */
/* endpoint == NULL or empty → disable OTLP. ──────────────────────── */
void bs_trace_otlp_set_endpoint(const char* endpoint_url);

#ifdef __cplusplus
}
#endif

#endif /* BS_TRACE_SPAN_H */
