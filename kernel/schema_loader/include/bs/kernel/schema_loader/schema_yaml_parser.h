#ifndef BS_KERNEL_SCHEMA_YAML_PARSER_H
#define BS_KERNEL_SCHEMA_YAML_PARSER_H

/*
 * C-ST-7 contract block:
 * Thread safety: reentrant (no global state).
 * Error semantics: int return; 0 on success, negative on parse error.
 * Platform notes: Manual line-based YAML parser (MVP).
 *                 TODO: 接入 libyaml 做完整的 YAML 解析
 */

#include <bs/kernel/schema/schema_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ── Parse a YAML schema file ──────────────────────────────────────── */
    int bs_schema_yaml_parse(const char* yaml_path,
                             struct bs_schema** out);

/* ── Parse a bundled YAML cache file (config-schema.bundled.yaml) ──── */
/* 与标准解析器相同，但额外跳过 generated_at / generated_from 等顶层键。
 * 适用于编译期聚合后的精简缓存格式。 */
    int bs_schema_yaml_parse_bundled(const char* yaml_path,
                                    struct bs_schema** out);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_YAML_PARSER_H */
