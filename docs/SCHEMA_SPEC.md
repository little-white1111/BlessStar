# BlessStar Compact Schema 格式规范

版本：1.0（第26天 MVP）

## 1. 概述

BlessStar Compact Schema 是 BlessStar 平台专有的精简 JSON Schema 格式。相比 JSON Schema Draft-07，它去掉继承/组合/引用等复杂特性，专注于配置校验的 80% 场景。

### 设计原则

- **简化必填**：五类校验规则（required / range / pattern / enum / custom_validator）直接内联在字段定义中
- **ai_hint 强制**：每个字段必须携带 `ai_hint`（4~1024 字符），用于 AI 辅助配置编辑
- **UI 元信息全量**：支持 label、description、placeholder、order 等 UI 提示
- **类型缩写**：短名称替代标准 JSON Schema 冗长写法
- **自定义校验器**：支持 expr 表达式字符串 + C 函数注册点双表达

## 2. 类型缩写表

| 缩写   | 含义         | 对应 Draft-07                               |
|--------|-------------|----------------------------------------------|
| `str`  | 字符串       | `{"type": "string"}`                         |
| `i32`  | 32 位整数    | `{"type": "integer", "minimum": -2147483648, "maximum": 2147483647}` |
| `i64`  | 64 位整数    | `{"type": "integer"}`                        |
| `f64`  | 64 位浮点数  | `{"type": "number"}`                         |
| `bool` | 布尔值       | `{"type": "boolean"}`                        |
| `arr`  | 数组         | `{"type": "array", "items": {...}}`          |
| `obj`  | 对象         | `{"type": "object", "properties": {...}}`    |
| `enum` | 枚举         | `{"type": "string", "enum": [...]}`          |

## 3. 字段内联属性参考表

所有属性均内联在同一对象中：

| 属性                       | 适用的类型            | 说明                                |
|----------------------------|----------------------|-------------------------------------|
| `type`                     | 全部                 | 类型缩写，见上表                     |
| `required`                 | 全部                 | boolean，是否必填                    |
| `range`                    | `i32`, `i64`, `f64`  | `{"min": 0, "max": 100}`           |
| `pattern`                  | `str`                | 正则表达式（ECMAScript 方言）        |
| `enum`                     | `enum`               | 枚举值数组 `["a", "b"]`            |
| `custom_validator`         | 全部                 | 自定义校验表达式或 C 函数名          |
| `ai_hint`                  | 全部                 | 必填！4~1024 字符 AI 辅助提示        |
| `nested_fields`            | `obj`                | 嵌套字段定义（递归）                 |
| `elem_type`                | `arr`                | 数组元素类型                        |
| `elem_fields`              | `arr` (elem=obj)     | 数组元素为 obj 时的嵌套字段定义      |
| `ui.label`                 | 全部                 | UI 显示标签                          |
| `ui.description`           | 全部                 | UI 详细说明                          |
| `ui.placeholder`           | 全部                 | UI 输入占位符                        |
| `ui.order`                 | 全部                 | UI 显示顺序（数字越小越靠前）         |

## 4. 完整 Schema 示例

```json
{
  "schema_id": "com.blessstar.user.profile",
  "version": "1.0",
  "fields": {
    "name": {
      "type": "str",
      "required": true,
      "pattern": "^.{1,64}$",
      "ai_hint": "用户姓名，最长64字符",
      "ui": { "label": "姓名", "description": "用户真实姓名", "order": 1 }
    },
    "age": {
      "type": "i32",
      "required": false,
      "range": { "min": 0, "max": 150 },
      "ai_hint": "年龄，0-150 岁之间",
      "ui": { "label": "年龄", "placeholder": "请输入年龄", "order": 2 }
    },
    "email": {
      "type": "str",
      "required": true,
      "pattern": "^\\S+@\\S+\\.\\S+$",
      "ai_hint": "电子邮箱地址",
      "custom_validator": "value ~= '.+@.+\\..+'",
      "ui": { "label": "邮箱", "order": 3 }
    },
    "role": {
      "type": "enum",
      "required": true,
      "enum": ["admin", "user", "guest"],
      "ai_hint": "用户角色：admin/user/guest",
      "ui": { "label": "角色", "order": 4 }
    },
    "salary": {
      "type": "f64",
      "required": false,
      "ai_hint": "薪酬数额，不含税",
      "custom_validator": "validate_salary",
      "ui": { "label": "薪酬", "order": 5 }
    },
    "address": {
      "type": "obj",
      "required": false,
      "ai_hint": "地址信息",
      "nested_fields": {
        "city": { "type": "str", "required": true, "ai_hint": "城市" },
        "street": { "type": "str", "required": false, "ai_hint": "街道" }
      },
      "ui": { "label": "地址", "order": 6 }
    },
    "tags": {
      "type": "arr",
      "required": false,
      "elem_type": "str",
      "ai_hint": "用户标签列表",
      "ui": { "label": "标签", "order": 7 }
    }
  },
  "ui": {
    "title": "用户配置",
    "description": "用户基本信息配置"
  }
}
```

## 5. JSON Schema Draft-07 双向转换规则

### BlessStar → Draft-07

| BlessStar                | Draft-07                                       |
|--------------------------|-------------------------------------------------|
| `type: "str"`           | `{"type": "string"}`                            |
| `type: "i32"`           | `{"type": "integer", "minimum": -2147483648, "maximum": 2147483647}` |
| `type: "i64"`           | `{"type": "integer"}`                           |
| `type: "f64"`           | `{"type": "number"}`                            |
| `type: "bool"`          | `{"type": "boolean"}`                           |
| `type: "arr"`           | `{"type": "array", "items": {...}}`             |
| `type: "obj"`           | `{"type": "object", "properties": {...}}`       |
| `type: "enum"`          | `{"type": "string", "enum": [...]}`             |
| `required: true`        | 加入 `required` 数组于父级                      |
| `range: {min, max}`     | `{"minimum": ..., "maximum": ...}`              |
| `pattern`               | `{"pattern": ...}`                              |
| `enum`                  | `{"enum": [...]}`                               |
| `ai_hint`               | `{"x-blessstar": {"ai_hint": ...}}`             |
| `ui.*`                  | `{"x-blessstar": {"ui": {...}}}`                |
| `custom_validator`      | `{"x-blessstar": {"custom_validator": ...}}`    |

### Draft-07 → BlessStar

- 缺失 `ai_hint`：报 WARN，生成空字符串占位
- 扩展槽 `x-blessstar` 之外的字段被忽略

## 6. 自定义校验器

### 表达式（expr）语法

```
expr       → or_expr
or_expr    → and_expr ( '||' and_expr )*
and_expr   → cmp_expr ( '&&' cmp_expr )*
cmp_expr   → arith_expr ( ('=='|'!='|'>'|'<'|'>='|'<=') arith_expr )?
arith_expr → '+' '-' unary_expr
           | unary_expr
unary_expr → '!' unary_expr
           | '(' expr ')'
           | 'value' | NUMBER | STRING
```

变量 `value` 代表被校验的配置值。表达式返回非零表示校验通过。

### C 函数注册点

```c
typedef int (*bs_custom_validator_fn)(const bs_value_t* val, char* err_buf, size_t err_sz);
```

通过 `bs_schema_register_validator(name, fn)` 注册。注册时检查是否存在同名 expr 实现，若两者均存在，要求语义一致性（MVP 级别：仅检查签名匹配）。

## 7. Schema 注册中心

按 `schema_id` + `version` 索引。每个注册的 Schema 包含：

- schema_id（字符串标识）
- version（语义化版本号，当前仅支持 "1.0"）
- 字段定义树
- UI 元信息
- 校验器

## 8. 简明语法参考

```json
// 最简字段
"field": { "type": "str", "ai_hint": "xxx" }

// 带校验规则
"field": { "type": "i32", "required": true, "range": { "min": 0, "max": 100 }, "ai_hint": "xxx" }

// 嵌套对象
"field": {
  "type": "obj",
  "nested_fields": { "sub": { "type": "str", "ai_hint": "xxx" } },
  "ai_hint": "xxx"
}

// 数组
"field": { "type": "arr", "elem_type": "str", "ai_hint": "xxx" }
// 元素为对象
"field": {
  "type": "arr",
  "elem_type": "obj",
  "elem_fields": { "id": { "type": "i32", "ai_hint": "xxx" } },
  "ai_hint": "xxx"
}
```

## 9. 格式版本兼容性

- 当前仅支持 Schema 格式版本 `"1.0"`
- 不向后兼容迁移
