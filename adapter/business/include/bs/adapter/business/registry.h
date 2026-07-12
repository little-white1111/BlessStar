#ifndef BS_ADAPTER_BUSINESS_REGISTRY_H
#define BS_ADAPTER_BUSINESS_REGISTRY_H

#include "bs/adapter/business/manifest.h"
#include "bs/app/sdk/normalizer_plugin.h"  /* BsNormalizerFn */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * bs_biz_registry_register — 向全局注册表注册一个业务系统.
 * @return 0 成功, -1 已存在, -2 参数错误
 */
int bs_biz_registry_register(const bs_manifest_t* manifest);

/**
 * bs_biz_registry_lookup — 按 biz_id 查询注册信息.
 * @return bs_manifest_t*  查询成功 (内部指针, 不可释放)
 *         NULL             未找到
 */
const bs_manifest_t* bs_biz_registry_lookup(const char* biz_id);

/**
 * bs_biz_registry_list — 获取所有已注册 biz_id.
 * @param out_ids  输出: 字符串数组 (调用者需通过 bs_biz_registry_free_list 释放)
 * @return 注册数量
 */
size_t bs_biz_registry_list(char*** out_ids);
void   bs_biz_registry_free_list(char** ids, size_t count);

/**
 * bs_biz_registry_register_normalizer — 注册指定业务的归一化器.
 * @return 0 成功, -1 biz_id 未注册
 */
int bs_biz_registry_register_normalizer(const char* biz_id, BsNormalizerFn fn);

/* ── 门禁注册 (依赖 CustomGateEntry, 需要 C++ 环境) ── */
#ifdef __cplusplus
struct CustomGateEntry;
int bs_biz_registry_register_gate(const char* biz_id,
                                   const struct CustomGateEntry* gate);
#endif

/**
 * bs_biz_get_normalizer — 获取注册的归一化器函数.
 * @return BsNormalizerFn  函数指针, 未注册时返回 NULL
 */
BsNormalizerFn bs_biz_get_normalizer(const char* biz_id);

/**
 * bs_biz_registry_count — 获取已注册业务系统数量.
 */
size_t bs_biz_registry_count(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_BUSINESS_REGISTRY_H */
