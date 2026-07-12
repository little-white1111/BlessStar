#include "bs/adapter/business/manifest.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── 辅助: 复制字符串 ─────────────────────────────────────────────── */

static char* strdup_safe(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* d = (char*)malloc(len + 1);
    if (!d) return NULL;
    memcpy(d, s, len + 1);
    return d;
}

/* ─── 销毁函数 ─────────────────────────────────────────────────────── */

void bs_manifest_destroy(bs_manifest_t* m) {
    if (!m) return;

    free(m->fields);
    m->fields = NULL;
    m->fields_count = 0;

    {
        bs_manifest_ai_labels_t* lbl = &m->ai_data.config_labels;
        for (size_t i = 0; i < lbl->count; i++) {
            free(lbl->keys[i]);
            free(lbl->values[i]);
        }
        free(lbl->keys);      lbl->keys = NULL;
        free(lbl->values);    lbl->values = NULL;
        lbl->count = 0;
    }

    for (size_t i = 0; i < m->ai_data.inverted_index_count; i++) {
        bs_manifest_inv_entry_t* e = &m->ai_data.inverted_index[i];
        for (size_t j = 0; j < e->keys_count; j++) free(e->config_keys[j]);
        free(e->config_keys);
    }
    free(m->ai_data.inverted_index);
    m->ai_data.inverted_index = NULL;
    m->ai_data.inverted_index_count = 0;

    for (size_t i = 0; i < m->ai_data.domain_shards_count; i++) {
        bs_manifest_domain_shard_t* s = &m->ai_data.domain_shards[i];
        for (size_t j = 0; j < s->keywords_count; j++) free(s->keywords[j]);
        free(s->keywords);
    }
    free(m->ai_data.domain_shards);
    m->ai_data.domain_shards = NULL;
    m->ai_data.domain_shards_count = 0;

    for (size_t i = 0; i < m->ai_data.skill_routes_count; i++) {
        bs_manifest_skill_route_t* r = &m->ai_data.skill_routes[i];
        for (size_t j = 0; j < r->tool_chain_count; j++) free(r->tool_chain[j]);
        free(r->tool_chain);
    }
    free(m->ai_data.skill_routes);
    m->ai_data.skill_routes = NULL;
    m->ai_data.skill_routes_count = 0;

    memset(m->biz_id, 0, sizeof(m->biz_id));
    memset(m->display_name, 0, sizeof(m->display_name));
    memset(m->sdk_version, 0, sizeof(m->sdk_version));
    memset(m->normalizer_lib_path, 0, sizeof(m->normalizer_lib_path));
}
