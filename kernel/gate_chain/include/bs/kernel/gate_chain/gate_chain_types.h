#ifndef BS_GATE_CHAIN_TYPES_H
#define BS_GATE_CHAIN_TYPES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Gate layer classification ─────────────────────────────────────── */
typedef enum {
    BS_GATE_LAYER_DEFAULT = 0,
    BS_GATE_LAYER_POLICY  = 1,
    BS_GATE_LAYER_CUSTOM  = 2,
    BS_GATE_LAYER_COUNT
} bs_gate_layer_t;

/* ── Gate node (extended with semantic indexing fields) ────────────── */
typedef struct bs_gate_node {
    char*  type;            /* "bs_gate_default" | "bs_condition" | "bs_meta_rule" | "bs_logic_and" | "bs_logic_or" | "bs_policy_attr" | "bs_custom_gate" */
    char*  id;              /* 唯一标识 */
    char*  field_key;       /* 关联 Schema 字段名（condition/meta_rule 类型） */
    char*  op;              /* 操作符："lt" | "gt" | "eq" | "ne" | "in" | "range" */
    char*  value;           /* 阈值或参数（JSON 字符串） */
    char** child_ids;       /* 子节点 ID 列表（logic_and/or 类型） */
    size_t child_count;
    char** do_ids;          /* DO 分支 ID 列表（condition 类型） */
    size_t do_count;

    /* ── OPT-08：语义索引字段 ── */
    char*  stable_key;      /* "domain:entity:field:layer:sub_category" */
    int    layer;           /* BS_GATE_LAYER_* */
    char*  sub_category;    /* "threshold"|"alert"|"approval"|"enum_check"|"format" */
    char*  domain;          /* 从 biz_index 继承 */
    char*  entity;          /* 从 biz_index 继承 */
} bs_gate_node_t;

/* ── Gate map slot (FNV-1a + open addressing hash) ─────────────────── */
typedef struct {
    char*  stable_key;
    size_t node_index;
} bs_gate_map_slot_t;

typedef struct {
    bs_gate_map_slot_t* slots;
    size_t              capacity;
    size_t              count;
} bs_gate_map_t;

/* ── Gate chain (with O(1) lookup map) ─────────────────────────────── */
typedef struct bs_gate_chain {
    char*            version;     /* "1.0" */
    bs_gate_node_t*  nodes;
    size_t           node_count;
    bs_gate_map_t*   map;         /* ★ stable_key→node_index，O(1) 查找 */
} bs_gate_chain_t;

void bs_gate_chain_free(bs_gate_chain_t* chain);

/* ── Gate map operations ───────────────────────────────────────────── */
int  bs_gate_map_create(bs_gate_map_t** out, size_t capacity);
void bs_gate_map_free(bs_gate_map_t* map);
int  bs_gate_map_insert(bs_gate_map_t* map, const char* stable_key, size_t node_index);
int  bs_gate_map_lookup(const bs_gate_map_t* map, const char* stable_key, size_t* out_index);
int  bs_gate_map_rebuild(bs_gate_map_t* map);

/* ── Gate chain upsert (idempotent write via stable_key) ───────────── */
int  bs_gate_chain_upsert(bs_gate_chain_t* chain, const bs_gate_node_t* node, size_t* out_index);

#ifdef __cplusplus
}
#endif

#endif /* BS_GATE_CHAIN_TYPES_H */
