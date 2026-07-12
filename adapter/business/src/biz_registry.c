#include "bs/adapter/business/registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define MUTEX_TYPE CRITICAL_SECTION
#define MUTEX_INIT(m) InitializeCriticalSection(&(m))
#define MUTEX_LOCK(m) EnterCriticalSection(&(m))
#define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#else
#include <pthread.h>
#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
#define MUTEX_LOCK(m) pthread_mutex_lock(&(m))
#define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
#define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#endif

/* ─── 全局注册表 ───────────────────────────────────────────────────── */

typedef struct biz_entry {
    char           biz_id[64];
    bs_manifest_t  manifest;          /* copy of original manifest */
    BsNormalizerFn normalizer_fn;
    int            has_gate;          /* C++ side only; placeholder */
} biz_entry_t;

#define MAX_BIZ 64

static biz_entry_t  g_entries[MAX_BIZ];
static size_t       g_count = 0;
static MUTEX_TYPE   g_mutex;
static int          g_mutex_inited = 0;

/* ─── 内部锁辅助 ────────────────────────────────────────────────────── */

static void ensure_lock(void) {
    if (!g_mutex_inited) {
        MUTEX_INIT(g_mutex);
        g_mutex_inited = 1;
    }
}

static void lock(void) { ensure_lock(); MUTEX_LOCK(g_mutex); }
static void unlock(void) { MUTEX_UNLOCK(g_mutex); }

/* ─── 查找辅助 ─────────────────────────────────────────────────────── */

static biz_entry_t* find_entry_locked(const char* biz_id) {
    if (!biz_id) return NULL;
    for (size_t i = 0; i < g_count; i++) {
        if (strcmp(g_entries[i].biz_id, biz_id) == 0)
            return &g_entries[i];
    }
    return NULL;
}

/* ─── 公有 API ─────────────────────────────────────────────────────── */

int bs_biz_registry_register(const bs_manifest_t* manifest) {
    if (!manifest || !manifest->biz_id[0]) return -2;

    lock();
    if (find_entry_locked(manifest->biz_id)) { unlock(); return -1; }
    if (g_count >= MAX_BIZ) { unlock(); return -2; }

    biz_entry_t* entry = &g_entries[g_count];
    memset(entry, 0, sizeof(biz_entry_t));

    memcpy(&entry->manifest, manifest, sizeof(bs_manifest_t));
    strncpy(entry->biz_id, manifest->biz_id, sizeof(entry->biz_id) - 1);
    entry->biz_id[sizeof(entry->biz_id) - 1] = '\0';
    entry->normalizer_fn = NULL;
    entry->has_gate = 0;

    g_count++;
    unlock();
    return 0;
}

const bs_manifest_t* bs_biz_registry_lookup(const char* biz_id) {
    lock();
    biz_entry_t* entry = find_entry_locked(biz_id);
    const bs_manifest_t* result = entry ? &entry->manifest : NULL;
    unlock();
    return result;
}

size_t bs_biz_registry_list(char*** out_ids) {
    lock();
    size_t count = g_count;
    if (!out_ids) { unlock(); return count; }

    *out_ids = (char**)calloc(count, sizeof(char*));
    if (!*out_ids) { unlock(); return 0; }

    size_t i;
    for (i = 0; i < count; i++) {
        (*out_ids)[i] = strdup(g_entries[i].biz_id);
        if (!(*out_ids)[i]) {
            for (size_t j = 0; j < i; j++) free((*out_ids)[j]);
            free(*out_ids);
            *out_ids = NULL;
            unlock();
            return 0;
        }
    }
    unlock();
    return count;
}

void bs_biz_registry_free_list(char** ids, size_t count) {
    if (!ids) return;
    for (size_t i = 0; i < count; i++) free(ids[i]);
    free(ids);
}

int bs_biz_registry_register_normalizer(const char* biz_id, BsNormalizerFn fn) {
    lock();
    biz_entry_t* entry = find_entry_locked(biz_id);
    if (!entry) { unlock(); return -1; }
    entry->normalizer_fn = fn;
    unlock();
    return 0;
}

#ifdef __cplusplus
int bs_biz_registry_register_gate(const char* biz_id,
                                   const struct CustomGateEntry* gate) {
    (void)gate;
    lock();
    biz_entry_t* entry = find_entry_locked(biz_id);
    if (!entry) { unlock(); return -1; }
    entry->has_gate = 1;
    unlock();
    return 0;
}
#endif

BsNormalizerFn bs_biz_get_normalizer(const char* biz_id) {
    lock();
    biz_entry_t* entry = find_entry_locked(biz_id);
    BsNormalizerFn fn = entry ? entry->normalizer_fn : NULL;
    unlock();
    return fn;
}

size_t bs_biz_registry_count(void) {
    lock();
    size_t count = g_count;
    unlock();
    return count;
}
