#ifndef BS_APP_SDK_EDITOR_BRIDGE_C_ABI_H
#define BS_APP_SDK_EDITOR_BRIDGE_C_ABI_H

/*
 * editor_bridge_c_abi.h — C ABI declarations for Electron Editor ↔ BlessStar SDK bridge
 *
 * These functions provide the C linkage needed by the Rust napi-rs native addon
 * to call into the C++ BlessStar SDK from the Electron renderer process.
 *
 * Thread safety: Not thread-safe. Calls must be sequenced by the Electron main process.
 * Memory: Returned char* values must be freed by the caller via free().
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Generic normalizer dispatch (plugin-based) ────────────────────────── */

/**
 * bs_normalizer_normalize_c - 调用已注册的归一化器插件执行归一化
 *
 * 内部通过 bs_normalizer_normalize() 分发到对应 vendor 的插件。
 * 插件需在首次调用前通过 bs_normalizer_register() 注册。
 *
 * @param vendor_id    业务系统标识（如 "livedesign"）
 * @param input_json   业务原始配置 JSON (UTF-8)
 * @param extra_json   附加数据 JSON（可为 NULL，如敏感配置）
 * @return Allocated Config v1 JSON string (caller must free()), or NULL on failure
 */
char* bs_normalizer_normalize_c(const char* vendor_id,
                                 const char* input_json,
                                 const char* extra_json);

/* ── AppSession lifecycle ─────────────────────────────────────────────── */

/**
 * app_session_create_c - Create an AppSession
 *
 * @param manifest_path Path to manifest file (may be NULL for in-memory only)
 * @return Opaque pointer to AppSession (void*), or NULL on failure
 */
void* app_session_create_c(const char* manifest_path);

/**
 * app_session_destroy_c - Destroy an AppSession created by app_session_create_c
 */
void app_session_destroy_c(void* session);

/**
 * app_session_get_ctx_c - Get the AttachContext* from an AppSession
 *
 * @return Opaque pointer to AttachContext (void*), or NULL
 */
void* app_session_get_ctx_c(void* session);

/**
 * app_session_is_ok_c - Check if AppSession initialized successfully
 *
 * @return 1 if ok, 0 otherwise
 */
int app_session_is_ok_c(void* session);

/* ── Config batch commit ──────────────────────────────────────────────── */

/**
 * config_commit_batch_c - Batch commit config entries
 *
 * @param session      AppSession handle (from app_session_create_c)
 * @param entries_json JSON array of {key, value} objects
 * @return Allocated Report JSON string (caller must free()), or NULL on failure
 */
char* config_commit_batch_c(void* session, const char* entries_json);

/* ── Gate rule registration（第34天 · GR-01：Gate 是基础设施）────────── */

/**
 * register_gate_rule_c - Register a Gate rule to AppSession gate_registry
 *
 * Registers a policy or custom gate rule that takes effect for all subsequent
 * config_commit_batch_c calls without needing restart or Session rebuild.
 *
 * @param session    AppSession handle (from app_session_create_c)
 * @param gate_type  "policy" or "custom"
 * @param rule_json  JSON string conforming to Gate rule protocol:
 *   Policy:  {"type":"policy","scenario":"production","metadata_rules":[...]}
 *   Custom:  {"type":"custom","gate_id":"...","scenario":"...","ast":{...}}
 * @return 0 on success, -1 on failure
 */
int register_gate_rule_c(void* session, const char* gate_type, const char* rule_json);

/* ── Agent index export ───────────────────────────────────────────────── */

/**
 * bs_agent_index_export_c - Export agent index files (domain_knowledge,
 *                           constraint_knowledge, field_semantics)
 *
 * @param schema_json   Schema JSON string (from get_registered_schema_json)
 * @param output_dir    Output directory path
 * @param business_name Business system name (e.g. "LiveDesign")
 * @return 0 on success, -1 on failure
 */
int bs_agent_index_export_c(const char* schema_json, const char* output_dir,
                            const char* business_name);

#ifdef __cplusplus
}
#endif

#endif /* BS_APP_SDK_EDITOR_BRIDGE_C_ABI_H */
