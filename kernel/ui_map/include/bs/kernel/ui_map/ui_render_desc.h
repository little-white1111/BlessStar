#ifndef BS_KERNEL_UI_MAP_UI_RENDER_DESC_H
#define BS_KERNEL_UI_MAP_UI_RENDER_DESC_H

/*
 * UIDL (UI Description Language) data structures.
 *
 * C-ST-7 contract block:
 * Thread safety: immutable after creation; read-only access safe.
 * Error semantics: int return; negative on error.
 * Platform notes: Pure C; outputs owned strings (caller must free).
 *
 * UIDL-01: UIDL abstract layer is Phase 2 — MVP directly outputs UIDL JSON.
 * UIDL-02: UIDL data structure reserves ai_layout_hint field (AI layout suggestion).
 * UIDL-03: Conditional visibility expressions processed on renderer side,
 *          kernel does NOT participate.
 * UIDL-04: MVP renderer target = Electron + React (RJSF).
 * UIDL-05: Custom controls extend via RJSF layered extension:
 *          widget -> field -> template.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ── Widget type enum ──────────────────────────────────────────────── */
    typedef enum bs_ui_widget_type
    {
        BS_UI_WIDGET_INPUT            = 0,
        BS_UI_WIDGET_SELECT           = 1,
        BS_UI_WIDGET_CHECKBOX         = 2,
        BS_UI_WIDGET_RADIO            = 3,
        BS_UI_WIDGET_SWITCH           = 4,
        BS_UI_WIDGET_TEXTAREA         = 5,
        BS_UI_WIDGET_NUMBER           = 6,
        BS_UI_WIDGET_DATEPICKER       = 7,
        BS_UI_WIDGET_FILE_UPLOAD      = 8,
        BS_UI_WIDGET_KEY_VALUE_TABLE  = 9,
        BS_UI_WIDGET_GROUP            = 10,
        BS_UI_WIDGET_TABLE            = 11,
        BS_UI_WIDGET_REPEATABLE_GROUP = 12,
        BS_UI_WIDGET_CUSTOM           = 13
    } bs_ui_widget_type_t;

/* ── Widget type string table (for JSON output) ────────────────────── */
    extern const char* bs_ui_widget_type_str(bs_ui_widget_type_t t);

/* ── Single control descriptor ─────────────────────────────────────── */
    typedef struct bs_ui_control
    {
        char*               field;           /* dot-notation, owned */
        bs_ui_widget_type_t widget;
        char*               label;           /* owned, may be NULL */
        int                 order;
        char*               group;           /* owned, may be NULL */
        char*               ai_layout_hint;  /* reserved, owned, may be NULL */
        char*               visibility;      /* conditional expr, owned, NULL = always */
        char*               default_value;   /* JSON encoded, owned, may be NULL */
        struct bs_ui_control* children;      /* nested sub-controls, owned array */
        size_t              children_count;
        char*               validation_ref;  /* owned, may be NULL */
    } bs_ui_control_t;

/* ── UIDL document (render description) ────────────────────────────── */
    typedef struct bs_ui_render_desc
    {
        int               uidl_version;
        char*             schema_ref;        /* owned */
        bs_ui_control_t*  controls;          /* owned array */
        size_t            controls_count;
    } bs_ui_render_desc_t;

/* ── Widget-to-string mapping ------------------------------------------
 * Maps each schema type to default UIDL widget (per decision table).
 *
 * | Schema type     | UIDL widget default | Overridable to              |
 * |-----------------|---------------------|-----------------------------|
 * | string          | input               | textarea, select, datepicker|
 * | string + enum   | select              | radio, checkbox             |
 * | i32 / i64       | number              | input                       |
 * | f64             | number              | —                           |
 * | bool            | checkbox            | switch, radio               |
 * | object          | group               | table                       |
 * | array           | repeatable_group    | table                       |
 * ------------------------------------------------------------------ */

/**
 * Map a schema type to default UIDL widget type.
 * @param schema_type  The bs_schema_type_t value
 * @param has_enum     Non-zero if the field has enum_values (for string+enum case)
 * @return Default bs_ui_widget_type_t for the given schema type
 */
    bs_ui_widget_type_t bs_ui_default_widget_for_type(int schema_type, int has_enum);

/* ── Lifecycle helpers ─────────────────────────────────────────────── */
    void bs_ui_control_free(bs_ui_control_t* ctl);
    void bs_ui_control_array_free(bs_ui_control_t* ctls, size_t count);
    void bs_ui_render_desc_free(bs_ui_render_desc_t* desc);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_UI_MAP_UI_RENDER_DESC_H */
