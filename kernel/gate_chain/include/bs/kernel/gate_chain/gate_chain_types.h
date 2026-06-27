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

/* ── Gate node (DAG: uses pointer arrays, not string ID references) ── */
typedef struct bs_gate_node {
    char*  type;            /* "bs_condition" | "bs_meta_rule" | "bs_logic_and"
                               | "bs_logic_or" | "bs_policy_attr" | "bs_custom_gate" */

    char*  id;              /* 唯一标识 */
    char*  field_key;       /* 关联 Schema 字段名（condition/meta_rule 类型） */
    char*  op;              /* 操作符："eq" | "ne" | "gt" | "lt" | "gte" | "lte" | "in" | "range" */
    char*  value;           /* 阈值或参数（JSON 字符串） */

    /* ── DAG 边：直接指针数组，递归遍历 O(N) ── */
    struct bs_gate_node** children;    /* AND/OR/等组合节点的子节点 */
    size_t child_count;
    struct bs_gate_node** do_nodes;    /* DO 分支子链（condition/action 类型） */
    size_t do_count;

    /* ── 语义索引字段 ── */
    char*  stable_key;      /* "domain:entity:field:layer:sub_category" */
    int    layer;           /* BS_GATE_LAYER_* */
    char*  sub_category;    /* "threshold"|"alert"|"approval"|"enum_check"|"format" */
    char*  domain;          /* 从 biz_index 继承 */
    char*  entity;          /* 从 biz_index 继承 */
} bs_gate_node_t;

/* ── Gate map slot (FNV-1a + open addressing hash, pointer-based) ──── */
typedef struct {
    char*           stable_key;
    bs_gate_node_t* node_ptr;   /* 直接指针，供 DAG 遍历 upsert */
} bs_gate_map_slot_t;

typedef struct {
    bs_gate_map_slot_t* slots;
    size_t              capacity;
    size_t              count;
} bs_gate_map_t;

/* ── Gate chain (pointer DAG + O(1) lookup map) ────────────────────── */
typedef struct bs_gate_chain {
    char*            version;     /* "1.0" */
    bs_gate_node_t*  root;        /* DAG 根节点，DFS 遍历入口 */
    bs_gate_map_t*   map;         /* ★ stable_key→node_ptr，O(1) 语义 upsert */
} bs_gate_chain_t;

/* ── Lifecycle ─────────────────────────────────────────────────────── */
void bs_gate_chain_free(bs_gate_chain_t* chain);
bs_gate_chain_t* bs_gate_chain_create(void);
bs_gate_node_t*  bs_gate_node_create(const char* type, const char* id);
void             bs_gate_node_free(bs_gate_node_t* node);

/* ── Gate map operations (pointer-based) ───────────────────────────── */
int  bs_gate_map_create(bs_gate_map_t** out, size_t capacity);
void bs_gate_map_free(bs_gate_map_t* map);
int  bs_gate_map_insert(bs_gate_map_t* map, const char* stable_key, bs_gate_node_t* node_ptr);
int  bs_gate_map_lookup(const bs_gate_map_t* map, const char* stable_key, bs_gate_node_t** out_ptr);
int  bs_gate_map_rebuild(bs_gate_map_t* map);

/* ── Gate chain upsert (idempotent write via stable_key) ───────────── */
/* Creates new or overwrites existing node by stable_key.               */
/* Returns pointer to the upserted node.                                */
bs_gate_node_t* bs_gate_chain_upsert_node(bs_gate_chain_t* chain, const bs_gate_node_t* src);

/* ── Gate chain node linking (attach children/do_nodes to a node) ──── */
int bs_gate_node_link_child(bs_gate_node_t* parent, bs_gate_node_t* child);
int bs_gate_node_link_do(bs_gate_node_t* parent, bs_gate_node_t* do_node);

/* ── Gate chain query ──────────────────────────────────────────────── */
bs_gate_node_t* bs_gate_chain_find(bs_gate_chain_t* chain, const char* stable_key);

/* ── DAG node count (DFS) ──────────────────────────────────────────── */
size_t bs_gate_chain_node_count(const bs_gate_chain_t* chain);

#ifdef __cplusplus
}
#endif

#endif /* BS_GATE_CHAIN_TYPES_H */
