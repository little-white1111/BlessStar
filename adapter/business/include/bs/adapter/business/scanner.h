#ifndef BS_ADAPTER_BUSINESS_SCANNER_H
#define BS_ADAPTER_BUSINESS_SCANNER_H

#include "bs/adapter/business/manifest.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * bs_biz_scanner_scan — 扫描目录下所有业务系统 manifest.
     *
     * @param base_dir  基础目录路径 (如 "business/")
     * @param out_manifests  输出: 解析后的 manifest 数组 (调用者需通过 bs_biz_scanner_free_result
     * 释放)
     * @param out_count      输出: 找到的 manifest 数量
     * @return 0  成功
     *         <0 错误码
     */
    int bs_biz_scanner_scan(const char* base_dir, bs_manifest_t** out_manifests, size_t* out_count);

    /**
     * bs_biz_scanner_free_result — 释放扫描结果.
     */
    void bs_biz_scanner_free_result(bs_manifest_t* manifests, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_BUSINESS_SCANNER_H */
