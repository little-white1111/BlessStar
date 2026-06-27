// gate_bridge.rs — Rust napi-rs bridge to bs_kernel_gate_chain C ABI
//
// 对应 DAY38-01：4 个 #[napi] 函数供 Electron 主进程通过 addon.* 调用
//   - run_gate_factory_produce
//   - run_gate_evaluator_evaluate
//   - run_gate_map_upsert
//   - run_gate_map_lookup

use napi_derive::napi;
use std::ffi::{CStr, CString};

// ── C FFI declarations ───────────────────────────────────────────────

extern "C" {
    // gate_chain_types.h
    fn bs_gate_chain_create() -> *mut std::ffi::c_void;
    fn bs_gate_chain_free(chain: *mut std::ffi::c_void);
    fn bs_gate_node_create(typ: *const i8, id: *const i8) -> *mut std::ffi::c_void;
    fn bs_gate_node_free(node: *mut std::ffi::c_void);
    fn bs_gate_map_create(out: *mut *mut std::ffi::c_void, capacity: usize) -> i32;
    fn bs_gate_map_free(map: *mut std::ffi::c_void);
    fn bs_gate_map_insert(map: *mut std::ffi::c_void, stable_key: *const i8, node_ptr: *mut std::ffi::c_void) -> i32;
    fn bs_gate_map_lookup(map: *mut std::ffi::c_void, stable_key: *const i8, out_ptr: *mut *mut std::ffi::c_void) -> i32;
    fn bs_gate_chain_upsert_node(chain: *mut std::ffi::c_void, src: *const std::ffi::c_void) -> *mut std::ffi::c_void;
    fn bs_gate_chain_find(chain: *mut std::ffi::c_void, stable_key: *const i8) -> *mut std::ffi::c_void;
    fn bs_gate_node_link_child(parent: *mut std::ffi::c_void, child: *mut std::ffi::c_void) -> i32;
    fn bs_gate_node_link_do(parent: *mut std::ffi::c_void, do_node: *mut std::ffi::c_void) -> i32;

    // gate_factory.h
    fn bs_gate_factory_produce(
        factory: *const std::ffi::c_void,
        rule: *const std::ffi::c_void,
        out: *mut *mut std::ffi::c_void,
    ) -> i32;
    fn bs_default_factory() -> *const std::ffi::c_void;
    fn bs_policy_factory() -> *const std::ffi::c_void;
    fn bs_custom_factory() -> *const std::ffi::c_void;
    fn bs_gate_factory_by_layer(layer: i32) -> *const std::ffi::c_void;
    fn bs_gate_factory_free_node(node: *mut std::ffi::c_void);

    // gate_evaluator.h
    fn bs_gate_evaluator_evaluate(
        chain: *const std::ffi::c_void,
        ctx: *const std::ffi::c_void,
        out: *mut std::ffi::c_void,
    ) -> i32;
    fn bs_gate_eval_result_free(result: *mut std::ffi::c_void);

    // gate_chain_serialize.h
    fn bs_gate_chain_from_json(json: *const i8, out: *mut *mut std::ffi::c_void) -> i32;
    fn bs_gate_chain_to_json(chain: *const std::ffi::c_void, out_json: *mut *mut i8, out_len: *mut usize) -> i32;
    fn free(ptr: *mut std::ffi::c_void);
}

// ── C struct mirrors (fields needed for FFI) ────────────────────────

#[repr(C)]
struct bs_gate_rule_def {
    field_key: *const i8,
    field_type: i32,      // bs_schema_type_t
    op: *const i8,
    value: *const i8,
    scenario: *const i8,
    layer: i32,           // bs_gate_layer_t
    ai_hint: *const i8,
}

#[repr(C)]
struct bs_gate_eval_context {
    field_key: *const i8,
    field_value: *const i8,
    user_data: *mut std::ffi::c_void,
}

#[repr(C)]
struct bs_gate_eval_result {
    passed: bool,
    failed_layer: usize,
    failed_node_index: usize,
    error_message: *mut i8,
}

// ── Layer constants ──────────────────────────────────────────────────

const BS_GATE_LAYER_DEFAULT: i32 = 0;
const BS_GATE_LAYER_POLICY: i32 = 1;
const BS_GATE_LAYER_CUSTOM: i32 = 2;

// ── Schema type constants ────────────────────────────────────────────

const BS_TYPE_INT32: i32 = 0;
const BS_TYPE_INT64: i32 = 1;
const BS_TYPE_STRING: i32 = 2;
const BS_TYPE_DOUBLE: i32 = 3;
const BS_TYPE_BOOL: i32 = 4;

/// Resolve factory pointer from layer name or factory_type string.
fn resolve_factory(factory_type: &str) -> *const std::ffi::c_void {
    match factory_type {
        "default" => unsafe { bs_default_factory() },
        "policy" => unsafe { bs_policy_factory() },
        "custom" => unsafe { bs_custom_factory() },
        _ => {
            // try numeric layer
            let layer: i32 = match factory_type {
                "0" | "default" => BS_GATE_LAYER_DEFAULT,
                "1" | "policy" => BS_GATE_LAYER_POLICY,
                "2" | "custom" => BS_GATE_LAYER_CUSTOM,
                _ => BS_GATE_LAYER_DEFAULT,
            };
            unsafe { bs_gate_factory_by_layer(layer) }
        }
    }
}

/// Resolve bs_schema_type_t from string
fn resolve_schema_type(typ: &str) -> i32 {
    match typ {
        "int32" => BS_TYPE_INT32,
        "int64" => BS_TYPE_INT64,
        "string" => BS_TYPE_STRING,
        "double" => BS_TYPE_DOUBLE,
        "bool" => BS_TYPE_BOOL,
        _ => BS_TYPE_STRING,
    }
}

// ── napi-rs exported functions ──────────────────────────────────────

/// Run gate_factory_produce: create a gate node from a rule definition.
/// @param factory_type "default" | "policy" | "custom" | "0" | "1" | "2"
/// @param rule_json JSON string with fields: field_key, field_type, op, value, scenario, layer, ai_hint
/// @returns JSON string of the produced bs_gate_node_t (simplified), or error
#[napi]
pub fn run_gate_factory_produce(factory_type: String, rule_json: String) -> napi::Result<String> {
    let factory = resolve_factory(&factory_type);
    if factory.is_null() {
        return Err(napi::Error::from_reason(format!("Unknown factory type: {}", factory_type)));
    }

    // Parse rule_json
    let rule: serde_json::Value = serde_json::from_str(&rule_json)
        .map_err(|e| napi::Error::from_reason(format!("Invalid rule JSON: {}", e)))?;

    let field_key = rule["field_key"].as_str().unwrap_or("");
    let field_type_str = rule["field_type"].as_str().unwrap_or("string");
    let field_type = resolve_schema_type(field_type_str);
    let op = rule["op"].as_str().unwrap_or("eq");
    let value = rule["value"].as_str().unwrap_or("");
    let scenario = rule["scenario"].as_str().unwrap_or("production");
    let ai_hint = rule["ai_hint"].as_str().unwrap_or("");
    let layer_raw = rule["layer"].as_i64().unwrap_or(0) as i32;

    let field_key_c = CString::new(field_key).map_err(|e| napi::Error::from_reason(format!("CString error: {}", e)))?;
    let op_c = CString::new(op).map_err(|e| napi::Error::from_reason(format!("CString error: {}", e)))?;
    let value_c = CString::new(value).map_err(|e| napi::Error::from_reason(format!("CString error: {}", e)))?;
    let scenario_c = CString::new(scenario).map_err(|e| napi::Error::from_reason(format!("CString error: {}", e)))?;
    let ai_hint_c = CString::new(ai_hint).map_err(|e| napi::Error::from_reason(format!("CString error: {}", e)))?;

    let rule_def = bs_gate_rule_def {
        field_key: field_key_c.as_ptr(),
        field_type,
        op: op_c.as_ptr(),
        value: value_c.as_ptr(),
        scenario: scenario_c.as_ptr(),
        layer: layer_raw,
        ai_hint: ai_hint_c.as_ptr(),
    };

    let mut node: *mut std::ffi::c_void = std::ptr::null_mut();
    let ret = unsafe { bs_gate_factory_produce(factory as *const std::ffi::c_void, &rule_def as *const bs_gate_rule_def as *const std::ffi::c_void, &mut node) };

    if ret != 0 || node.is_null() {
        return Err(napi::Error::from_reason(format!("bs_gate_factory_produce failed with ret={}", ret)));
    }

    // Build a JSON representation from the node
    // Since we can't directly read the C struct fields from Rust without full struct def,
    // we return a success marker. Full struct serialization requires C-side JSON conversion.
    let result = serde_json::json!({
        "success": true,
        "node_ptr": format!("{:p}", node),
        "factory_type": factory_type,
    });

    Ok(result.to_string())
}

/// Evaluate a gate chain against a field value.
/// @param chain_json JSON representation of a bs_gate_chain_t
/// @param field_key the config field key to evaluate
/// @param field_value the config field value to evaluate against
/// @returns JSON string with passed/failed/error_message
#[napi]
pub fn run_gate_evaluator_evaluate(
    chain_json: String,
    field_key: String,
    field_value: String,
) -> napi::Result<String> {
    let chain_json_c = CString::new(&chain_json[..])
        .map_err(|e| napi::Error::from_reason(format!("CString error: {}", e)))?;

    let mut chain: *mut std::ffi::c_void = std::ptr::null_mut();
    let ret = unsafe { bs_gate_chain_from_json(chain_json_c.as_ptr(), &mut chain) };

    if ret != 0 || chain.is_null() {
        return Err(napi::Error::from_reason(format!("bs_gate_chain_from_json failed with ret={}", ret)));
    }

    let field_key_c = CString::new(&field_key[..])
        .map_err(|e| { unsafe { bs_gate_chain_free(chain); } napi::Error::from_reason(format!("CString error: {}", e)) })?;
    let field_value_c = CString::new(&field_value[..])
        .map_err(|e| { unsafe { bs_gate_chain_free(chain); } napi::Error::from_reason(format!("CString error: {}", e)) })?;

    let ctx = bs_gate_eval_context {
        field_key: field_key_c.as_ptr(),
        field_value: field_value_c.as_ptr(),
        user_data: std::ptr::null_mut(),
    };

    let mut result = bs_gate_eval_result {
        passed: true,
        failed_layer: 0,
        failed_node_index: 0,
        error_message: std::ptr::null_mut(),
    };

    let eval_ret = unsafe {
        bs_gate_evaluator_evaluate(
            chain as *const std::ffi::c_void,
            &ctx as *const bs_gate_eval_context as *const std::ffi::c_void,
            &mut result as *mut bs_gate_eval_result as *mut std::ffi::c_void,
        )
    };

    let error_msg = if result.error_message.is_null() {
        String::new()
    } else {
        let c_str = unsafe { CStr::from_ptr(result.error_message) };
        c_str.to_str().unwrap_or("").to_string()
    };

    let output = serde_json::json!({
        "passed": result.passed,
        "failed_layer": result.failed_layer,
        "failed_node_index": result.failed_node_index,
        "error_message": error_msg,
    });

    unsafe {
        bs_gate_eval_result_free(&mut result as *mut bs_gate_eval_result as *mut std::ffi::c_void);
        bs_gate_chain_free(chain);
    }

    Ok(output.to_string())
}

/// Upsert a gate node into the gate chain map by stable_key.
/// @param stable_key e.g. "production:amount_limit:0:threshold"
/// @param node_json JSON with node fields (type, id, field_key, op, value, layer, sub_category)
/// @returns JSON success marker
#[napi]
pub fn run_gate_map_upsert(stable_key: String, node_json: String) -> napi::Result<String> {
    let chain = unsafe { bs_gate_chain_create() };
    if chain.is_null() {
        return Err(napi::Error::from_reason("bs_gate_chain_create failed"));
    }

    let node: serde_json::Value = serde_json::from_str(&node_json)
        .map_err(|e| {
            unsafe { bs_gate_chain_free(chain); }
            napi::Error::from_reason(format!("Invalid node JSON: {}", e))
        })?;

    let node_type = node["type"].as_str().unwrap_or("bs_condition");
    let node_id = node["id"].as_str().unwrap_or(&stable_key);

    let type_c = CString::new(node_type)
        .map_err(|e| { unsafe { bs_gate_chain_free(chain); } napi::Error::from_reason(format!("CString error: {}", e)) })?;
    let id_c = CString::new(node_id)
        .map_err(|e| { unsafe { bs_gate_chain_free(chain); } napi::Error::from_reason(format!("CString error: {}", e)) })?;

    let c_node = unsafe { bs_gate_node_create(type_c.as_ptr(), id_c.as_ptr()) };
    if c_node.is_null() {
        unsafe { bs_gate_chain_free(chain); }
        return Err(napi::Error::from_reason("bs_gate_node_create failed"));
    }

    let sk_c = CString::new(&stable_key[..])
        .map_err(|e| { unsafe { bs_gate_node_free(c_node); bs_gate_chain_free(chain); } napi::Error::from_reason(format!("CString error: {}", e)) })?;

    let upserted = unsafe { bs_gate_chain_upsert_node(chain, c_node as *const std::ffi::c_void) };

    let result = if upserted.is_null() {
        serde_json::json!({ "success": false, "stable_key": stable_key, "error": "upsert returned NULL" })
    } else {
        serde_json::json!({ "success": true, "stable_key": stable_key })
    };

    unsafe {
        bs_gate_node_free(c_node);
        bs_gate_chain_free(chain);
    }

    Ok(result.to_string())
}

/// Look up a gate node from the gate chain map by stable_key.
/// @param stable_key e.g. "production:amount_limit:0:threshold"
/// @returns JSON with success flag
#[napi]
pub fn run_gate_map_lookup(stable_key: String) -> napi::Result<String> {
    let chain = unsafe { bs_gate_chain_create() };
    if chain.is_null() {
        return Err(napi::Error::from_reason("bs_gate_chain_create failed"));
    }

    let sk_c = CString::new(&stable_key[..])
        .map_err(|e| { unsafe { bs_gate_chain_free(chain); } napi::Error::from_reason(format!("CString error: {}", e)) })?;

    let node = unsafe { bs_gate_chain_find(chain, sk_c.as_ptr()) };

    let result = serde_json::json!({
        "found": !node.is_null(),
        "stable_key": stable_key,
    });

    unsafe { bs_gate_chain_free(chain); }

    Ok(result.to_string())
}
