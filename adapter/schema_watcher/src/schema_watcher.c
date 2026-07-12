#include <bs/adapter/schema_watcher/schema_watcher.h>
#include <bs/kernel/common/bs_log.h>
#include <bs/kernel/schema_loader/schema_loader.h>

#include <stdlib.h>
#include <string.h>

/* Windows compatibility: use _stat instead of stat */
#ifdef _MSC_VER
#include <sys/stat.h>
#define bs_stat _stat
#else
#include <sys/stat.h>
#define bs_stat stat
#endif

/* ── Domain ID for logging (schema_watcher) ────────────────────────── */
#define BS_LOG_DOMAIN_SCHEMA_WATCHER 0x1201

/* ── Internal state ────────────────────────────────────────────────── */
struct bs_schema_watcher {
    char*                    yaml_path;       /* owned */
    struct bs_schema_loader* loader;           /* not owned */
    long long                last_mtime;      /* last known mtime */
    long long                last_size;       /* last known file size */
};

/* ── Get file modification time (Windows-compatible) ───────────────── */
static long long get_file_mtime(const char* path)
{
    if (!path) return -1;
    struct bs_stat st;
    if (bs_stat(path, &st) != 0) return -1;

#ifdef _MSC_VER
    return (long long)st.st_mtime;
#else
    return (long long)st.st_mtime;
#endif
}

/* ── Get file size ─────────────────────────────────────────────────── */
static long long get_file_size(const char* path)
{
    if (!path) return -1;
    struct bs_stat st;
    if (bs_stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

/* ── Create ────────────────────────────────────────────────────────── */
struct bs_schema_watcher* bs_schema_watcher_create(
    const char* yaml_path,
    struct bs_schema_loader* loader)
{
    if (!yaml_path || !loader) return NULL;

    struct bs_schema_watcher* watcher =
        (struct bs_schema_watcher*)calloc(1, sizeof(struct bs_schema_watcher));
    if (!watcher) return NULL;

    watcher->yaml_path = strdup(yaml_path);
    if (!watcher->yaml_path) { free(watcher); return NULL; }

    watcher->loader = loader;
    watcher->last_mtime = get_file_mtime(yaml_path);
    watcher->last_size  = get_file_size(yaml_path);

    return watcher;
}

/* ── Destroy ───────────────────────────────────────────────────────── */
void bs_schema_watcher_destroy(struct bs_schema_watcher* watcher)
{
    if (!watcher) return;
    free(watcher->yaml_path);
    free(watcher);
}

/* ── Non-blocking single poll check (ADR 第3217-3248行) ─────────────── */
/*
 * 非阻塞单次检查：比较 mtime/size，有变更则触发 reload，否则立即返回。
 * 由主机语言（Go/Java）的协程调度器定时调用。
 *
 * 返回: 0  = 无变更
 *       1  = 已触发重载
 *       -1 = 错误（文件不存在等）
 */
int bs_schema_watcher_check_and_reload(struct bs_schema_watcher* watcher)
{
    if (!watcher || !watcher->yaml_path) return -1;

    long long mtime = get_file_mtime(watcher->yaml_path);
    long long size  = get_file_size(watcher->yaml_path);

    if (mtime < 0) {
        /* File disappeared */
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_WATCHER, BS_LOG_WARN,
                     "SchemaWatcher: file not found (%s), cannot reload",
                     watcher->yaml_path);
        return -1;
    }

    if (mtime != watcher->last_mtime || size != watcher->last_size) {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_WATCHER, BS_LOG_INFO,
                     "SchemaWatcher: file change detected (%s), triggering reload",
                     watcher->yaml_path);
        watcher->last_mtime = mtime;
        watcher->last_size  = size;
        return bs_schema_loader_reload(watcher->loader);
    }

    return 0;  /* no change */
}
