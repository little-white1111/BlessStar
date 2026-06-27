use napi_derive::napi;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

mod gate_bridge;
mod shm_notify;
mod shm_manager;

/// UIDL node structure matching TypeScript UIDLNode interface
#[derive(Serialize, Deserialize, Clone)]
#[serde(rename_all = "snake_case")]
pub struct UidlNode {
    pub widget: String,
    pub label: String,
    pub key: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub required: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub placeholder: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub default_value: Option<serde_json::Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub options: Option<Vec<UidlOption>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub children: Option<Vec<UidlNode>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub order: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub visibility: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ai_layout_hint: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub validation: Option<UidlValidation>,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct UidlOption {
    pub label: String,
    pub value: String,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct UidlValidation {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub min: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pattern: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_length: Option<i32>,
}

/// UIDL document (top-level structure matching UIDLDocument)
#[derive(Serialize, Deserialize)]
pub struct UidlDocument {
    pub render_type: String,
    pub version: String,
    pub title: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub description: Option<String>,
    pub fields: Vec<UidlNode>,
}

/// Schema JSON field structure from bs_config_declare_get_schema_json()
#[derive(Deserialize)]
struct SchemaField {
    key: String,
    #[serde(rename = "type")]
    field_type: String,
    #[serde(default)]
    default: Option<String>,
    #[serde(default)]
    description: Option<String>,
    #[serde(default)]
    required: Option<bool>,
}

/// Schema JSON top-level structure
#[derive(Deserialize)]
struct SchemaJson {
    #[serde(default)]
    fields: Vec<SchemaField>,
}

/// Map schema type to UIDL widget
fn type_to_widget(typ: &str, has_bool_default: bool) -> &'static str {
    match typ {
        "bool" => "checkbox",
        "int32" | "int64" => "number",
        "double" => "number",
        "string" if has_bool_default => "checkbox",
        _ => "input",
    }
}

/// Build domain group label from dotted key prefix (generic — no domain-specific knowledge)
fn domain_label(prefix: &str) -> String {
    prefix.split('.').last().unwrap_or(prefix).to_string()
}

/// TypeScript type string mapping
fn type_to_ts_type(typ: &str) -> &'static str {
    match typ {
        "int32" | "int64" => "number",
        "double" => "number",
        "bool" => "boolean",
        _ => "string",
    }
}

/// Parse a BlessStar schema JSON into UIDL JSON, grouped by domain.
#[napi]
pub fn schema_to_uidl(schema_json: String) -> napi::Result<String> {
    let schema: SchemaJson = serde_json::from_str(&schema_json)
        .map_err(|e| napi::Error::from_reason(format!("解析 Schema JSON 失败: {}", e)))?;

    // Group fields by second-level prefix (e.g. "livedesign.room")
    let mut groups: HashMap<String, Vec<&SchemaField>> = HashMap::new();
    for field in &schema.fields {
        // Extract prefix like "livedesign.room" from "livedesign.room.id"
        let parts: Vec<&str> = field.key.splitn(3, '.').collect();
        let prefix = if parts.len() >= 2 {
            format!("{}.{}", parts[0], parts[1])
        } else {
            field.key.clone()
        };
        groups.entry(prefix).or_default().push(field);
    }

    let mut uidl_fields: Vec<UidlNode> = Vec::new();
    let mut order: i32 = 1;

    // Sort group keys for deterministic output
    let mut group_keys: Vec<&String> = groups.keys().collect();
    group_keys.sort();

    for gkey in group_keys {
        let fields = &groups[gkey];

        // Create a group node per domain
        let mut children: Vec<UidlNode> = Vec::new();
        let mut child_order: i32 = 1;

        for field in fields {
            let has_bool_default = field.default.as_deref() == Some("true")
                || field.default.as_deref() == Some("false");
            let widget = type_to_widget(&field.field_type, has_bool_default);

            let description = field.description.clone().unwrap_or_default();

            let node = UidlNode {
                widget: widget.to_string(),
                label: description.clone(),
                key: field.key.clone(),
                required: field.required,
                placeholder: Some(description.clone()),
                description: Some(description),
                default_value: field.default.as_ref().map(|s| {
                    match widget {
                        "number" => {
                            if let Ok(n) = s.parse::<f64>() {
                                serde_json::Value::Number(serde_json::Number::from_f64(n).unwrap_or(serde_json::Number::from(0)))
                            } else {
                                serde_json::Value::String(s.clone())
                            }
                        }
                        "checkbox" => {
                            serde_json::Value::Bool(s == "true")
                        }
                        _ => serde_json::Value::String(s.clone()),
                    }
                }),
                options: None,
                children: None,
                order: Some(child_order),
                visibility: None,
                ai_layout_hint: Some(format!("type:{}", field.field_type)),
                validation: match widget {
                    "number" => Some(UidlValidation {
                        min: None,
                        max: None,
                        pattern: None,
                        max_length: None,
                    }),
                    _ => Some(UidlValidation {
                        min: None,
                        max: None,
                        pattern: None,
                        max_length: Some(255),
                    }),
                },
            };
            children.push(node);
            child_order += 1;
        }

        uidl_fields.push(UidlNode {
            widget: "group".to_string(),
            label: domain_label(gkey).to_string(),
            key: gkey.clone(),
            required: None,
            placeholder: None,
            description: Some(format!("{} 配置组", domain_label(gkey))),
            default_value: None,
            options: None,
            children: Some(children),
            order: Some(order),
            visibility: None,
            ai_layout_hint: None,
            validation: None,
        });
        order += 1;
    }

    let doc = UidlDocument {
        render_type: "dynamic_form".to_string(),
        version: "1.0.0".to_string(),
        title: "配置管理".to_string(),
        description: Some(format!(
            "共 {} 个配置组，{} 个配置项",
            uidl_fields.len(),
            schema.fields.len()
        )),
        fields: uidl_fields,
    };

    serde_json::to_string(&doc)
        .map_err(|e| napi::Error::from_reason(format!("序列化 UIDL 失败: {}", e)))
}

/// 获取已注册的 Schema JSON 字符串。
/// 内部调用 bs_config_declare_get_schema_json() C ABI。
/// 返回的 JSON 包含所有已注册字段的 key、type、default、description。
/// 如果尚无 Schema 注册，返回错误。
#[napi]
pub fn get_registered_schema_json() -> napi::Result<String> {
    let mut out_json: *mut c_char = std::ptr::null_mut();
    let mut out_len: usize = 0;

    let ret = unsafe {
        bs_config_declare_get_schema_json(&mut out_json, &mut out_len)
    };

    if ret != 0 || out_json.is_null() {
        return Err(napi::Error::from_reason("尚无已注册的 Schema 字段"));
    }

    let result = unsafe {
        let c_str = CStr::from_ptr(out_json);
        c_str.to_str()
            .map_err(|e| napi::Error::from_reason(format!("Schema JSON 不是合法 UTF-8: {}", e)))?
            .to_string()
    };

    unsafe { free(out_json) };

    Ok(result)
}

use std::ffi::{c_char, CStr, CString};

extern "C" {
    fn bs_config_read(key: *const c_char) -> *const c_char;
    fn bs_config_write(key: *const c_char, value: *const c_char) -> i32;
    fn bs_config_declare_get_schema_json(out_json: *mut *mut c_char, out_len: *mut usize) -> i32;
    fn bs_config_declare_field_c(key: *const c_char, typ: i32,
                                  default_str: *const c_char,
                                  description: *const c_char,
                                  required: i32) -> i32;
    fn free(ptr: *mut c_char);
}

/// 从 BlessStar 运行时值存储读取单个配置值。
/// 内部调用 bs_config_read() C ABI，返回字符串值。
/// 如果 key 不存在或值为空，返回 None。
#[napi]
pub fn read_bless_star_config(key: String) -> Option<String> {
    let c_key = CString::new(key).ok()?;
    let ptr = unsafe { bs_config_read(c_key.as_ptr()) };
    if ptr.is_null() {
        return None;
    }
    let c_str = unsafe { CStr::from_ptr(ptr) };
    let s = c_str.to_str().ok()?.to_string();
    if s.is_empty() { None } else { Some(s) }
}

/// 写入单个配置值到 BlessStar 运行时值存储。
/// 内部调用 bs_config_write() C ABI。
/// key 必须已通过 register_schema_field_ffi() 声明，否则返回错误。
#[napi]
pub fn write_bless_star_config(key: String, value: String) -> napi::Result<()> {
    let c_key = CString::new(key)
        .map_err(|e| napi::Error::from_reason(format!("key 含空字节: {}", e)))?;
    let c_val = CString::new(value)
        .map_err(|e| napi::Error::from_reason(format!("value 含空字节: {}", e)))?;
    let ret = unsafe { bs_config_write(c_key.as_ptr(), c_val.as_ptr()) };
    if ret != 0 {
        return Err(napi::Error::from_reason(
            "bs_config_write 失败：key 未声明或参数无效"
        ));
    }
    Ok(())
}

/* ══════════════════════════════════════════════════════════════════
 * Editor Bridge FFI — C ABI FFI declarations
 * ══════════════════════════════════════════════════════════════════ */

extern "C" {
    fn bs_normalizer_normalize_c(vendor_id: *const c_char,
                                  input_json: *const c_char,
                                  extra_json: *const c_char) -> *mut c_char;
    fn app_session_create_c(manifest_path: *const c_char) -> *mut std::ffi::c_void;
    fn app_session_destroy_c(session: *mut std::ffi::c_void);
    fn app_session_get_ctx_c(session: *mut std::ffi::c_void) -> *mut std::ffi::c_void;
    fn app_session_is_ok_c(session: *mut std::ffi::c_void) -> i32;
    fn config_commit_batch_c(session: *mut std::ffi::c_void, entries_json: *const c_char) -> *mut c_char;
    fn register_gate_rule_c(session: *mut std::ffi::c_void, gate_type: *const c_char, rule_json: *const c_char) -> i32;
    fn bs_agent_index_export_c(schema_json: *const c_char, output_dir: *const c_char,
                               business_name: *const c_char) -> i32;
}

/// normalizer_normalize_ffi — 通过 NormalizerRegistry 调用已注册的归一化器
///
/// @param vendor_id  业务系统标识（如 "livedesign"）
/// @param input_json 业务原始配置 JSON
/// @param extra_json 附加数据 JSON（可选，如敏感配置）
/// @return 归一化后的 Config v1 JSON 字符串，失败返回 None
#[napi]
pub fn normalizer_normalize_ffi(vendor_id: String, input_json: String, extra_json: Option<String>) -> Option<String> {
    let c_vendor = CString::new(vendor_id).ok()?;
    let c_input = CString::new(input_json).ok()?;
    let c_extra = extra_json
        .and_then(|s| CString::new(s).ok())
        .unwrap_or_default();

    let ptr = unsafe { bs_normalizer_normalize_c(c_vendor.as_ptr(), c_input.as_ptr(), c_extra.as_ptr()) };
    if ptr.is_null() {
        return None;
    }
    let result = unsafe {
        let c_str = CStr::from_ptr(ptr);
        let s = c_str.to_str().ok()?.to_string();
        free(ptr);
        Some(s)
    };
    result
}

/// app_session_create_ffi — 创建 AppSession，返回 handle（opaque i64）
#[napi]
pub fn app_session_create_ffi(manifest_path: Option<String>) -> Option<i64> {
    let c_path = manifest_path
        .and_then(|p| CString::new(p).ok());

    let ptr = unsafe {
        match &c_path {
            Some(p) => app_session_create_c(p.as_ptr()),
            None => app_session_create_c(std::ptr::null()),
        }
    };

    if ptr.is_null() {
        return None;
    }
    Some(ptr as i64)
}

/// app_session_destroy_ffi — 销毁 AppSession
#[napi]
pub fn app_session_destroy_ffi(handle: i64) {
    if handle == 0 { return; }
    let ptr = handle as *mut std::ffi::c_void;
    unsafe { app_session_destroy_c(ptr); }
}

/// app_session_get_ctx_ffi — 获取 AttachContext* 作为 i64 handle
#[napi]
pub fn app_session_get_ctx_ffi(handle: i64) -> i64 {
    if handle == 0 { return 0; }
    let session = handle as *mut std::ffi::c_void;
    let ctx = unsafe { app_session_get_ctx_c(session) };
    ctx as i64
}

/// app_session_is_ok_ffi — 检查 AppSession 是否就绪
#[napi]
pub fn app_session_is_ok_ffi(handle: i64) -> bool {
    if handle == 0 { return false; }
    let session = handle as *mut std::ffi::c_void;
    unsafe { app_session_is_ok_c(session) != 0 }
}

/// config_commit_batch_ffi — 批量提交配置变更
///
/// @param session_handle AppSession handle（从 app_session_create_ffi 获取）
/// @param entries_json JSON 数组 [{key, value}, ...]
/// @return Report JSON 字符串，失败返回 None
#[napi]
pub fn config_commit_batch_ffi(session_handle: i64, entries_json: String) -> Option<String> {
    if session_handle == 0 { return None; }
    let session = session_handle as *mut std::ffi::c_void;
    let c_json = CString::new(entries_json).ok()?;

    let ptr = unsafe { config_commit_batch_c(session, c_json.as_ptr()) };
    if ptr.is_null() {
        return None;
    }
    let result = unsafe {
        let c_str = CStr::from_ptr(ptr);
        let s = c_str.to_str().ok()?.to_string();
        free(ptr);
        Some(s)
    };
    result
}

/// register_gate_rule_ffi — 注册 Gate 规则到 AppSession（第34天 · GR-01）
///
/// @param session_handle AppSession handle（从 app_session_create_ffi 获取）
/// @param gate_type "policy" 或 "custom"
/// @param rule_json Gate 规则 JSON 字符串
/// @return true 成功，false 失败
#[napi]
pub fn register_gate_rule_ffi(session_handle: i64, gate_type: String, rule_json: String) -> bool {
    if session_handle == 0 { return false; }
    let session = session_handle as *mut std::ffi::c_void;
    let c_type = match CString::new(gate_type) { Ok(t) => t, Err(_) => return false };
    let c_json = match CString::new(rule_json) { Ok(j) => j, Err(_) => return false };

    let ret = unsafe { register_gate_rule_c(session, c_type.as_ptr(), c_json.as_ptr()) };
    ret == 0
}

/// agent_index_export_ffi — 导出 Agent 索引文件
#[napi]
pub fn agent_index_export_ffi(schema_json: String, output_dir: String, business_name: String) -> bool {
    let c_schema = match CString::new(schema_json) {
        Ok(s) => s,
        Err(_) => return false,
    };
    let c_dir = match CString::new(output_dir) {
        Ok(d) => d,
        Err(_) => return false,
    };
    let c_name = match CString::new(business_name) {
        Ok(n) => n,
        Err(_) => return false,
    };

    let ret = unsafe {
        bs_agent_index_export_c(c_schema.as_ptr(), c_dir.as_ptr(), c_name.as_ptr())
    };
    ret == 0
}

/// register_schema_field_ffi — 专题五 A4: 注册单个 Schema 字段（真实 C ABI）
///
/// @param key 字段标识符（如 "livedesign.room.id"）
/// @param field_type bs_field_type_t 枚举值（0=INT32, 1=INT64, 2=STRING, 3=DOUBLE, 4=BOOL）
/// @param default_str 默认值字符串
/// @param description 字段描述
/// @param required 是否必填
/// @return 成功返回 true，失败返回错误
#[napi]
pub fn register_schema_field_ffi(
    key: String,
    field_type: i32,
    default_str: Option<String>,
    description: Option<String>,
    required: Option<bool>,
) -> napi::Result<()> {
    let c_key = CString::new(key)
        .map_err(|e| napi::Error::from_reason(format!("key 含空字节: {}", e)))?;
    let c_default = CString::new(default_str.unwrap_or_default())
        .unwrap_or_default();
    let c_desc = CString::new(description.unwrap_or_default())
        .unwrap_or_default();
    let req = if required.unwrap_or(false) { 1 } else { 0 };

    let ret = unsafe {
        bs_config_declare_field_c(
            c_key.as_ptr(),
            field_type,
            c_default.as_ptr(),
            c_desc.as_ptr(),
            req,
        )
    };

    if ret != 0 {
        return Err(napi::Error::from_reason(
            "bs_config_declare_field_c 失败：参数无效或 key 为空"
        ));
    }
    Ok(())
}

/* ══════════════════════════════════════════════════════════════════
 * Metrics export — Prometheus text format via bs_metrics_export_prometheus
 * DAY38-10: 5 core counters/gauges
 * ══════════════════════════════════════════════════════════════════ */

extern "C" {
    fn bs_metrics_export_prometheus(registry: *mut std::ffi::c_void, out_json: *mut *mut i8) -> usize;
    fn bs_metric_registry_create() -> *mut std::ffi::c_void;
    fn bs_metric_registry_destroy(registry: *mut std::ffi::c_void);
}

/// Export all metrics in Prometheus text format.
/// Returns text like:
///   # HELP config_writes_total Total config write operations
///   # TYPE config_writes_total counter
///   config_writes_total 42
#[napi]
pub fn export_metrics_prometheus() -> napi::Result<String> {
    let registry = unsafe { bs_metric_registry_create() };
    if registry.is_null() {
        return Err(napi::Error::from_reason("bs_metric_registry_create failed"));
    }

    let mut out_json: *mut i8 = std::ptr::null_mut();
    let len = unsafe { bs_metrics_export_prometheus(registry, &mut out_json) };

    let result = if out_json.is_null() || len == 0 {
        String::new()
    } else {
        let slice = unsafe { std::slice::from_raw_parts(out_json as *const u8, len) };
        let s = String::from_utf8_lossy(slice).to_string();
        unsafe { free(out_json as *mut std::ffi::c_void); }
        s
    };

    unsafe { bs_metric_registry_destroy(registry); }
    Ok(result)
}

/* ══════════════════════════════════════════════════════════════════
 * Trace span export — JSON format via bs_trace_export_json
 * DAY38-11: 8 instrumented sites
 * ══════════════════════════════════════════════════════════════════ */

extern "C" {
    fn bs_trace_export_json(out_json: *mut *mut i8) -> usize;
    fn bs_trace_export_free(json: *mut i8);
}

/// Export all finished trace spans as JSON string.
/// OTLP-compatible span format with traceId/spanId/parentSpanId.
#[napi]
pub fn export_trace_spans() -> napi::Result<String> {
    let mut out_json: *mut i8 = std::ptr::null_mut();
    let len = unsafe { bs_trace_export_json(&mut out_json) };

    let result = if out_json.is_null() || len == 0 {
        "{\"spans\":[]}".to_string()
    } else {
        let slice = unsafe { std::slice::from_raw_parts(out_json as *const u8, len) };
        String::from_utf8_lossy(slice).to_string()
    };

    if !out_json.is_null() {
        unsafe { bs_trace_export_free(out_json); }
    }

    Ok(result)
}

/* ══════════════════════════════════════════════════════════════════
 * Auth token verify — delegates to C-side bs_auth_token_verify
 * DAY38-12: token format + expiry check
 * ══════════════════════════════════════════════════════════════════ */

extern "C" {
    fn bs_auth_token_verify(token: *const i8) -> i32;
}

/// Verify an auth token for validity.
/// Returns JSON: {"success":true/false,"valid":1/0}
#[napi]
pub fn auth_verify_token(token: String) -> napi::Result<String> {
    let c_token = std::ffi::CString::new(token).unwrap_or_default();
    let valid = unsafe { bs_auth_token_verify(c_token.as_ptr()) };
    Ok(format!(r#"{{"success":true,"valid":{}}}"#, valid).into())
}

/* ══════════════════════════════════════════════════════════════════
 * execute_query — 触发 C++ 侧 QueryExecutorRegistry::ApplyChanges
 * D38-3-INV-03: 外部系统触发走 Config Backflow
 * ══════════════════════════════════════════════════════════════════ */

extern "C" {
    fn bs_query_executor_apply_changes(config_key: *const i8) -> i32;
}

/// 触发查询执行器。IPC 工具由 executeTool 调度。
/// 参数: { "config_key": "query.audience" }
/// 返回: { "success": true, "status": 0/1/-1 }
#[napi]
pub fn execute_query(config_key: String) -> napi::Result<String> {
    if config_key.is_empty() {
        return Ok(r#"{"success":false,"status":-1,"error":"empty config_key"}"#.into());
    }
    let c_key = std::ffi::CString::new(config_key.as_str())
        .map_err(|e| napi::Error::from_reason(format!("CString error: {}", e)))?;
    let status = unsafe { bs_query_executor_apply_changes(c_key.as_ptr()) };
    Ok(format!(r#"{{"success":true,"status":{}}}"#, status).into())
}

/* ══════════════════════════════════════════════════════════════════
 * 专题十二：单文件持久化存储 FFI
 * ══════════════════════════════════════════════════════════════════ */

/// 从 manifest.json 读取 data_file 字段，解析为 configs.json 的完整路径
fn resolve_data_file_from_manifest(manifest_path: &str) -> Option<String> {
    let content = std::fs::read_to_string(manifest_path).ok()?;
    let parsed: serde_json::Value = serde_json::from_str(&content).ok()?;
    let data_file = parsed.get("data_file")?.as_str()?;
    if data_file.is_empty() {
        return None;
    }

    // 获取 manifest 所在目录
    let manifest_dir = std::path::Path::new(manifest_path).parent()?;
    let full_path = manifest_dir.join(data_file);
    Some(full_path.to_string_lossy().to_string())
}

extern "C" {
    fn bs_config_persist_write_c(file_path: *const c_char) -> i32;
    fn bs_config_persist_load_c(file_path: *const c_char) -> i32;
}

/// config_persist_write_ffi — 持久化写入：将 runtime_values 写入 configs.json
///
/// @param manifest_path manifest.json 的完整路径
/// @return true 成功，false 失败
#[napi]
pub fn config_persist_write_ffi(manifest_path: String) -> bool {
    let data_path = match resolve_data_file_from_manifest(&manifest_path) {
        Some(p) => p,
        None => return false,
    };
    let c_path = match CString::new(data_path) {
        Ok(p) => p,
        Err(_) => return false,
    };
    let ret = unsafe { bs_config_persist_write_c(c_path.as_ptr()) };
    ret == 0
}

/// config_persist_load_ffi — 持久化加载：从 configs.json 读取并加载到 runtime_values
///
/// @param manifest_path manifest.json 的完整路径
/// @return true 成功，false 失败
#[napi]
pub fn config_persist_load_ffi(manifest_path: String) -> bool {
    let data_path = match resolve_data_file_from_manifest(&manifest_path) {
        Some(p) => p,
        None => return false,
    };
    let c_path = match CString::new(data_path) {
        Ok(p) => p,
        Err(_) => return false,
    };
    let ret = unsafe { bs_config_persist_load_c(c_path.as_ptr()) };
    ret == 0
}
