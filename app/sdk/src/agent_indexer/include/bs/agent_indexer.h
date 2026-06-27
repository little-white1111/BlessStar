#ifndef BS_AGENT_INDEXER_H
#define BS_AGENT_INDEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <bs/kernel/schema/schema_types.h>
#include <bs/kernel/schema/schema_registry.h>
#include <bs/kernel/gate_chain/gate_chain_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bs_agent_index_config {
    const char* output_dir;        /* 输出目录，如 ".cursor/agents/biz-finance/" */
    const char* business_name;     /* 业务系统名称 */
    bool        include_ai_hints;  /* 是否导出 ai_hint */
    bool        include_gate_chain;/* 是否导出 Gate 约束 */
    bool        include_biz_index; /* ★ OPT-08: 是否导出 biz_semantic_index.json */
    const char* format;            /* 输出格式："json"（默认）或 "compact"（管道分隔） */
} bs_agent_index_config_t;

/* 完整导出：Schema + Gate */
int bs_agent_index_export(
    bs_schema_registry_t*  reg,
    const bs_gate_chain_t* chain,
    const bs_agent_index_config_t* config
);

/* 仅导出 Schema（无 Gate 场景） */
int bs_agent_index_export_schema_only(
    bs_schema_registry_t*  reg,
    const bs_agent_index_config_t* config
);

/**
 * bs_agent_index_needs_rebuild() — 专题五 C1: 检查索引是否需要重建
 * @reg:       Schema registry
 * @config:    索引配置
 * @return:    1 = 需要重建，0 = 无需重建（指纹匹配），-1 = 错误
 *
 * 通过对比 Schema registry 的当前哈希指纹与上一次写入的
 * biz_index_fingerprint.txt 判断是否需要重建。
 */
int bs_agent_index_needs_rebuild(
    bs_schema_registry_t*  reg,
    const bs_agent_index_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* BS_AGENT_INDEXER_H */
