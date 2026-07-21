#ifndef BS_ADAPTER_BUSINESS_MANIFEST_H
#define BS_ADAPTER_BUSINESS_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* ─── 字段声明 ─────────────────────────────────────────────────────── */

    typedef struct bs_manifest_field
    {
        char key[64];
        char type[16]; /* string / integer / boolean / float */
        char default_str[256];
        char description[512];
        int  required; /* 0/1 */
    } bs_manifest_field_t;

    /* ─── AI 元数据 ────────────────────────────────────────────────────── */

    /* config_labels: 并行数组 */
    typedef struct bs_manifest_ai_labels
    {
        char** keys;
        char** values;
        size_t count;
    } bs_manifest_ai_labels_t;

    /* inverted_index 条目 */
    typedef struct bs_manifest_inv_entry
    {
        char   keyword[128];
        char** config_keys;
        size_t keys_count;
    } bs_manifest_inv_entry_t;

    /* domain_shard 条目 */
    typedef struct bs_manifest_domain_shard
    {
        char   domain_name[128];
        char** keywords;
        size_t keywords_count;
        char   domain_description[1024];
    } bs_manifest_domain_shard_t;

    /* skill_route 条目 */
    typedef struct bs_manifest_skill_route
    {
        char   prefix[64];
        char   description[512];
        char** tool_chain;
        size_t tool_chain_count;
        int    priority;
    } bs_manifest_skill_route_t;

    typedef struct bs_manifest_ai_data
    {
        bs_manifest_ai_labels_t     config_labels;
        bs_manifest_inv_entry_t*    inverted_index;
        size_t                      inverted_index_count;
        bs_manifest_domain_shard_t* domain_shards;
        size_t                      domain_shards_count;
        bs_manifest_skill_route_t*  skill_routes;
        size_t                      skill_routes_count;
    } bs_manifest_ai_data_t;

    /* ─── 顶层 manifest ────────────────────────────────────────────────── */

    typedef struct bs_manifest
    {
        char biz_id[64];
        char display_name[128];
        char sdk_version[64]; /* semver range, e.g. ">=1.0.0 <2.0.0" */

        bs_manifest_field_t* fields;
        size_t               fields_count;

        bs_manifest_ai_data_t ai_data;

        char normalizer_lib_path[256]; /* "" = no normalizer */
    } bs_manifest_t;

    /* ─── 生命周期 ─────────────────────────────────────────────────────── */

    void bs_manifest_destroy(bs_manifest_t* m);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_BUSINESS_MANIFEST_H */
