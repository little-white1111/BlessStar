#ifndef BS_STORAGE_PERSIST_H
#define BS_STORAGE_PERSIST_H

/*
 * ADR-workspace加固 — PersistStoreStorage 声明。
 * 适用: 生产环境，需要 WAL + 版本追踪 + 崩溃恢复的场景。
 * 不变量 #5 (生产默认): 无 BS_TESTING 且无 BSTORAGE 覆盖时默认使用。
 *
 * 注意: 此后端依赖 io_worker 线程，不兼容沙箱环境。
 */

#include "bs/adapter/workspace/storage_backend.h"
#include <bs/adapter/persistence/attach_store.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 创建 PersistStoreStorage 实例。
 * 包装 BsAttachStore，提供原子写入 + WAL + CRC32 + epoch 版本管理。
 *
 * @param project_root  工作区根路径
 * @param store         已打开的 BsAttachStore 实例（调用者管理生命周期）
 */
StorageBackend* PersistStoreStorage_new(const char* project_root,
                                         BsAttachStore* store);

#ifdef __cplusplus
}
#endif

#endif /* BS_STORAGE_PERSIST_H */
