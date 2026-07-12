#ifndef BS_ADAPTER_SCHEMA_WATCHER_H
#define BS_ADAPTER_SCHEMA_WATCHER_H

/*
 * C-ST-7 contract block:
 * Thread safety: NOT thread-safe; callers must serialize access.
 * Error semantics: int return; 0=no change, 1=reloaded, negative=error.
 * Platform notes: Uses _stat (Windows) / stat (POSIX) polling (non-blocking).
 *                 TODO: 引入 fsnotify/libuv 做原生文件系统事件监听
 *
 * Design principle (ADR 第11条不变量):
 *   C 层保持纯净，不做线程/异步操作。阻塞负担交给主机语言（Go/Java）的协程调度器。
 *   主机层每 2s 调用一次 bs_schema_watcher_check_and_reload() 完成轮询。
 */

#include <bs/kernel/schema_loader/schema_loader.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Opaque handle. */
    struct bs_schema_watcher;

/* ── Lifecycle ─────────────────────────────────────────────────────── */
    struct bs_schema_watcher* bs_schema_watcher_create(
        const char* yaml_path,
        struct bs_schema_loader* loader);

    void bs_schema_watcher_destroy(struct bs_schema_watcher* watcher);

/* ── Non-blocking single poll check (ADR 第3217-3248行) ─────────────── */
/*
 * 非阻塞单次检查：比较 mtime/size，有变更则触发 reload，否则立即返回。
 * 由主机语言（Go/Java）的协程调度器定时调用，C 层不做线程管理。
 *
 * 返回: 0  = 无变更
 *       1  = 已触发重载
 *       -1 = 错误（文件不存在等）
 */
    int bs_schema_watcher_check_and_reload(struct bs_schema_watcher* watcher);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_SCHEMA_WATCHER_H */
