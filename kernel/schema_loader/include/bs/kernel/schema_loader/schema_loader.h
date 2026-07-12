#ifndef BS_KERNEL_SCHEMA_LOADER_H
#define BS_KERNEL_SCHEMA_LOADER_H

/*
 * C-ST-7 contract block:
 * Thread safety: NOT thread-safe; callers must serialize access.
 * Error semantics: int return; 0 on success, negative on error.
 * Platform notes: Pure C; YAML parsing uses manual line-based parser
 *                 (TODO: replace with libyaml for production).
 */

#include <bs/kernel/schema/schema_types.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Opaque handle. */
    struct bs_schema_loader;

/* ── Lifecycle ─────────────────────────────────────────────────────── */
    struct bs_schema_loader* bs_schema_loader_create(const char* yaml_path);
    struct bs_schema_loader* bs_schema_loader_create_bundled(const char* yaml_path);
    void                     bs_schema_loader_destroy(struct bs_schema_loader* loader);

/* ── Reload ────────────────────────────────────────────────────────── */
    int bs_schema_loader_reload(struct bs_schema_loader* loader);
    int bs_schema_loader_reload_bundled(struct bs_schema_loader* loader);

/* ── Access active schema (read-only) ──────────────────────────────── */
    const struct bs_schema* bs_schema_loader_get_active(
        const struct bs_schema_loader* loader);

/* ── Hot-switch callback registration ──────────────────────────────── */
    typedef void (*bs_schema_switch_callback)(
        const struct bs_schema* new_schema, void* userdata);

    int bs_schema_loader_on_switch(
        struct bs_schema_loader* loader,
        bs_schema_switch_callback callback,
        void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_LOADER_H */
