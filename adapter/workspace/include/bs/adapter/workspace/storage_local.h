#ifndef BS_STORAGE_LOCAL_H
#define BS_STORAGE_LOCAL_H

/*
 * ADR-workspace加固 — LocalDirectStorage 声明。
 * 适用: 沙箱/测试/轻量单用户部署。
 * 不变量 #4 (测试无侵入): BS_TESTING 下自动启用。
 */

#include "bs/adapter/workspace/storage_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 创建 LocalDirectStorage 实例。
 * 使用裸文件 I/O (fopen/fread/fwrite/_commit) 读写。
 * 不提供版本追踪或崩溃安全。
 */
StorageBackend* LocalDirectStorage_new(const char* project_root);

#ifdef __cplusplus
}
#endif

#endif /* BS_STORAGE_LOCAL_H */
