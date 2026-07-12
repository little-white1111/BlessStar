#ifndef BS_STORAGE_BACKEND_H
#define BS_STORAGE_BACKEND_H

/*
 * C-ST-7 contract block:
 * Thread safety: StorageBackend implementations must be reentrant;
 *                callers serialize per-workspace access.
 * Error semantics: negative errno codes on error.
 * Platform notes: N/A.
 *
 * ADR-workspace加固 — 不变量 #1 (存储后端透明性):
 *   bs_workspace_build / bs_workspace_export 不得感知 StorageBackend 实现。
 * ADR-workspace加固 — 不变量 #3 (路径安全):
 *   所有后端必须拒绝 ".." 路径穿越。
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** StorageBackend 虚函数表 — 所有文件 I/O 操作通过此接口 */
typedef struct StorageBackend {
    /** Implementation context (opaque, set by constructor). */
    void* impl_ctx;

    /** 读取文件全部内容。返回 0=成功, -ENOENT, -EIO, -ENOMEM */
    int (*read)(void* ctx, const char* path,
                uint8_t** out_data, size_t* out_len);
    /** 写入文件（覆盖）。返回 0=成功, -EIO, -ENOMEM */
    int (*write)(void* ctx, const char* path,
                 const uint8_t* data, size_t len);
    /** 检查文件是否存在。返回 1=存在, 0=不存在, <0=错误 */
    int (*exists)(void* ctx, const char* path);
    /** 删除文件。返回 0=成功, -ENOENT, -EIO */
    int (*remove_fn)(void* ctx, const char* path);
    /** 销毁后端，释放资源 */
    void (*destroy)(void* ctx);
} StorageBackend;

/**
 * 通用路径安全校验 — 拒绝 ".." 路径穿越。
 * 所有 StorageBackend 实现必须在 read/write/exists/remove 前调用此函数。
 * @return 0=安全, -EINVAL=路径包含 ".."
 */
int storage_path_safe(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* BS_STORAGE_BACKEND_H */
