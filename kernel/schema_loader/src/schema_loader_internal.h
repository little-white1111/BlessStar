#ifndef BS_KERNEL_SCHEMA_LOADER_INTERNAL_H
#define BS_KERNEL_SCHEMA_LOADER_INTERNAL_H

/*
 * Internal data structures for the SchemaLoader module.
 * Not part of the public API.
 */

#include <bs/kernel/schema/gold_standard.h>
#include <bs/kernel/schema_loader/schema_loader.h>
#include <bs/kernel/schema_loader/schema_yaml_parser.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ── Schema field (gold-standard filtered) ─────────────────────────── */
    struct bs_schema_field
    {
        char*       qualified_name;  /* owned, dot-notation */
        char*       value;           /* owned, string representation */
        bs_schema_type_t type;
        int         required;
    };

/* ── Schema (active/parsed representation) ─────────────────────────── */
    struct bs_schema
    {
        char*                   version;       /* owned */
        struct bs_schema_field* fields;        /* owned array */
        size_t                  field_count;
    };

/* ── SchemaLoader internal state ───────────────────────────────────── */
    struct bs_schema_loader
    {
        char*                    yaml_path;        /* owned */
        struct bs_schema*        active_schema;    /* owned, may be NULL */
        bs_schema_switch_callback on_switch;
        void*                    switch_userdata;
    };

/* ── Internal helpers ──────────────────────────────────────────────── */
    void bs_schema_free(struct bs_schema* schema);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_LOADER_INTERNAL_H */
