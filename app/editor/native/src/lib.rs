use napi_derive::napi;
use serde::{Deserialize, Serialize};

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

/// Parse a BlessStar schema JSON into UIDL JSON.
/// Currently returns mock data; will be replaced with real schema parsing.
#[napi]
pub fn schema_to_uidl(schema_json: String) -> napi::Result<String> {
    // TODO: VP-5 落实后替换为真实 schema→UIDL 转换
    // 当前返回模拟 UIDL 演示数据
    let doc = UidlDocument {
        render_type: "dynamic_form".to_string(),
        version: "1.0.0".to_string(),
        title: "数据库连接配置".to_string(),
        description: Some("配置数据库连接参数".to_string()),
        fields: vec![
            UidlNode {
                widget: "input".to_string(),
                label: "主机地址".to_string(),
                key: "host".to_string(),
                required: Some(true),
                placeholder: Some("localhost".to_string()),
                description: None,
                default_value: Some(serde_json::Value::String("localhost".to_string())),
                options: None,
                children: None,
                order: Some(1),
                visibility: None,
                ai_layout_hint: None,
                validation: Some(UidlValidation {
                    min: None,
                    max: None,
                    pattern: None,
                    max_length: Some(255),
                }),
            },
        ],
    };

    serde_json::to_string(&doc).map_err(|e| {
        napi::Error::from_reason(format!("序列化 UIDL 失败: {}", e))
    })
}
