#ifndef BS_ADAPTER_BUSINESS_VERSION_CHECK_H
#define BS_ADAPTER_BUSINESS_VERSION_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * bs_version_compatible — 检查 sdk_version 是否满足 manifest 要求的 semver range.
 *
 * @param sdk_version    框架当前 SDK 版本, 如 "1.2.3"
 * @param required_range manifest 要求的版本范围, 如 ">=1.0.0 <2.0.0"
 * @return 0  兼容
 *         -1 不兼容
 *         -2 解析错误
 */
int bs_version_compatible(const char* sdk_version, const char* required_range);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_BUSINESS_VERSION_CHECK_H */
