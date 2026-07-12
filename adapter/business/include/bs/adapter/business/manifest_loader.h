#ifndef BS_ADAPTER_BUSINESS_MANIFEST_LOADER_H
#define BS_ADAPTER_BUSINESS_MANIFEST_LOADER_H

#include "bs/adapter/business/manifest.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * bs_manifest_load_from_json — 从 JSON 字符串完整解析 manifest.
 *
 * 解析 manifest.json 中所有字段:
 *   - biz_id, display_name, sdk_version
 *   - fields[] (key/type/default_str/description/required)
 *   - ai_data (config_labels, inverted_index, domain_shards, skill_routes)
 *   - normalizer_lib_path
 *
 * @param json      UTF-8 JSON 字符串 (调用者保持生命周期)
 * @param out       输出: 解析后的 manifest (调用者通过 bs_manifest_destroy 释放)
 * @return 0  成功
 *         -1 JSON 解析错误 (缺少必需字段 / 格式错误)
 *         -2 内存分配失败
 */
int bs_manifest_load_from_json(const char* json, bs_manifest_t* out);

/**
 * bs_manifest_load_from_file — 从文件完整解析 manifest.
 *
 * @param file_path manifest.json 的完整路径
 * @param out       输出: 解析后的 manifest
 * @return 0  成功
 *         -1 文件不存在或无法读取
 *         -2 JSON 解析错误
 *         -3 内存分配失败
 */
int bs_manifest_load_from_file(const char* file_path, bs_manifest_t* out);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_BUSINESS_MANIFEST_LOADER_H */
