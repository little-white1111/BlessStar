/*
 * adapter_stubs.c — BlessStar 适配器层最小桩实现
 *
 * bs_config_declare_ffi 薄库包含 app_session.cpp，它调用以下适配器函数。
 * config_commit_batch_c 定义在 config_reload_session.cpp（含 attach 整链），
 * 不纳入薄库，此处提供空桩。
 * Rust FFI bridge 不会实际触发这些路径，返回失败/空值即可。
 */

#include <stddef.h>

typedef struct AttachContext AttachContext;
typedef struct RegistryFacade RegistryFacade;
typedef struct IoFacade IoFacade;

/* ── adapter: attach_context ──────────────────────────────────────── */

AttachContext* bs_adapter_attach_ctx_create(void) { return NULL; }
void           bs_adapter_attach_ctx_destroy(AttachContext* ctx) { (void)ctx; }
RegistryFacade* bs_adapter_attach_ctx_registry(AttachContext* ctx) { (void)ctx; return NULL; }
int  bs_adapter_attach_ctx_open_persist_store(AttachContext* ctx, const char* p) {
    (void)ctx; (void)p; return -1;
}

/* ── adapter: registry_bootstrap ──────────────────────────────────── */

int bs_adapter_registry_bootstrap_begin_ctx(AttachContext* ctx) { (void)ctx; return -1; }
int bs_adapter_registry_bootstrap_freeze_ctx(AttachContext* ctx) { (void)ctx; return -1; }

/* ── kernel: io_facade ─────────────────────────────────────────────── */

IoFacade* bs_io_facade_create(RegistryFacade* facade) { (void)facade; return NULL; }
void      bs_io_facade_destroy(IoFacade* io) { (void)io; }

/* ── sdk: config_reload_session 桩（不纳入薄库） ──────────────────── */

char* config_commit_batch_c(void* ctx, const char* entries_json) {
    (void)ctx; (void)entries_json;
    return NULL;
}
