#ifndef BS_APP_SDK_NORMALIZER_PLUGIN_H
#define BS_APP_SDK_NORMALIZER_PLUGIN_H

/*
 * normalizer_plugin.h — 归一化器插件 C ABI 接口
 *
 * 业务系统通过此接口向 BlessStar SDK 注册自定义配置归一化器。
 * 注册后，bs_normalizer_normalize() 按 vendor_id 分发到对应插件。
 *
 * 使用示例（业务侧 C/C++）：
 *   int my_normalize(const char* vendor_id, const char* input,
 *                    const char* extra, char** out, size_t* out_len) {
 *       // 将 input JSON 归一化为 BlessStar v1 JSON
 *       *out = strdup("{...}");
 *       *out_len = strlen(*out);
 *       return 0;
 *   }
 *   // 注册：
 *   bs_normalizer_register("my_business", my_normalize);
 *
 * 线程安全：bs_normalizer_register / normalize / unregister 内部加锁。
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BsNormalizerFn — 归一化函数指针
 *
 * @param vendor_id   业务系统标识（如 "livedesign"）
 * @param input_json  业务原始配置 JSON 字符串（UTF-8，调用方保证生命周期）
 * @param extra_json  附加数据 JSON（可为 NULL，如敏感配置）
 * @param out_json    输出参数，接收归一化后的 v1 JSON（调用者须 free()）
 * @param out_len     输出参数，JSON 字符串字节数（不含 NUL）
 * @return            0 成功，-1 失败
 */
typedef int (*BsNormalizerFn)(const char* vendor_id,
                               const char* input_json,
                               const char* extra_json,
                               char** out_json,
                               size_t* out_len);

/**
 * bs_normalizer_register() — 注册一个归一化器插件
 *
 * @param vendor_id  业务系统唯一标识（如 "livedesign"），不可为 NULL 或空串
 * @param fn         归一化函数指针，不可为 NULL
 * @return           0 成功，-1 失败（参数无效）
 *
 * 幂等性：同一 vendor_id 重复注册会覆盖旧函数。调用方应保证
 * vendor_id 不与其他业务系统冲突。
 */
int bs_normalizer_register(const char* vendor_id, BsNormalizerFn fn);

/**
 * bs_normalizer_normalize() — 调用已注册的归一化器执行归一化
 *
 * @param vendor_id   业务系统标识
 * @param input_json  业务原始配置 JSON
 * @param extra_json  附加数据 JSON（可为 NULL）
 * @param out_json    输出参数，接收 v1 JSON 字符串（调用者须 free()）
 * @param out_len     输出参数，JSON 长度
 * @return            0 成功，-1 失败（vendor_id 未注册或归一化失败）
 */
int bs_normalizer_normalize(const char* vendor_id,
                             const char* input_json,
                             const char* extra_json,
                             char** out_json,
                             size_t* out_len);

/**
 * bs_normalizer_unregister() — 注销归一化器（测试/清理用）
 *
 * @param vendor_id  业务系统标识
 * @return           0 成功，-1 失败（vendor_id 不存在）
 */
int bs_normalizer_unregister(const char* vendor_id);

#ifdef __cplusplus
}
#endif

#endif /* BS_APP_SDK_NORMALIZER_PLUGIN_H */
