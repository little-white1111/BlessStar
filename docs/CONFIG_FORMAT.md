# BlessStar Config v1 格式参考

> 参考风格：Linux man page —— 精确的格式规范、字段语义清晰

BlessStar Config v1 是 BlessStar 流水线消费的标准配置格式。所有异构配置（厂商文件、手动编辑、程序生成）最终需归一化为此格式。

## 格式概览

```json
{
  "kernel_version":  "0.3.0",
  "adapter_version": "0.3.0",
  "manual_requirements": [],
  "instructions": [
    {
      "type":     "test",
      "name":     "approval-chain-v1",
      "metadata": {
        "subject_code": "1001.02",
        "tax_rate":     "10",
        "max_amount":   "500000"
      }
    }
  ]
}
```

## Schema 定义

完整 JSON Schema (draft-07) 位于：

```
tools/normalize/blessstar_config_v1.schema.json
```

## 根对象字段

### `kernel_version`（必需，string）

Kernel 版本标识。必须与 `KernelBuiltinRequirements.kernel_version` 在 `attach` 时刻对齐。

- **校验方式**：`bs_adapter_requirement_filter_check_kernel_version()`
- **预期值**：取决于编译时的 `blESSSTAR_KERNEL_VERSION`（当前一般为 `"0.3.0"`）

### `adapter_version`（必需，string）

Adapter 版本标识。必须与 Adapter 编译时版本对齐。

- **校验方式**：`bs_adapter_requirement_filter_check_adapter_version()`

### `manual_requirements`（可选，string[]）

扩展指令类型列表。用于提供 Kernel builtin 之外的自定义指令类型。

- **合并规则**：与 builtin 类型合并，builtin 同名优先
- **安全限制**：最大 256 项（`BS_JSON_MAX_MANUAL_ITEMS`）
- **可省略**：不提供时为 `[]`

### `instructions`（必需，array of object）

IR 指令载荷。Kernel 实际执行的内容。

**数组限制**：最多 2048 条指令（`BS_JSON_MAX_INSTRUCTIONS`）

#### 每条 instruction 的字段

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `type` | string | ✅ | 指令类型。Kernel builtin 允许的类型 + `manual_requirements` 中扩展的类型 |
| `name` | string | ✅ | 指令名称。业务逻辑标识，在同一配置中应唯一 |
| `metadata` | object | ❌ | 指令参数键值对。所有值必须为 string 类型 |

**`metadata` 约束**：
- 必须是扁平 map（`{ "key": "value" }`），不支持嵌套
- 所有值必须为 string 类型（数字需显式转字符串，如 `"500000"`）
- `additionalProperties: false` —— 不允许未在 schema 中定义的字段

### metadata 消费

`metadata` 字段在 BlessStar 全链路中透明穿通。App SDK 提供 `ConfigSessionReader` 以结构化方式消费：

```cpp
#include <bs/app/sdk/config_session_reader.h>

// 从 AppSession 构造 Reader
bs::app::ConfigSessionReader reader(session.ctx());

// 查询指定指令的 metadata（自动从 Gate 缓存读取，零额外解析）
const IRInstruction* instr = reader.GetInstruction("mem://mycfg", "vat-rate");
if (instr) {
    const char* subject = bs_ir_instruction_get_metadata(instr, "subject_code");
    const char* rate    = bs_ir_instruction_get_metadata(instr, "tax_rate");
    // subject = "1001.02", rate = "10"
}

// 线程安全：可从 Watch callback 中调用
// 热更新感知：配置变更后自动刷新缓存
```

详见 [API_REFERENCE.md §11 ConfigSessionReader](./API_REFERENCE.md#11-configsessionreader--metadata-消费)。


## 完整限制表

| 限制项 | 上限 | 触发行为 |
|--------|------|---------|
| 指令数量 | 2048 (`BS_JSON_MAX_INSTRUCTIONS`) | 解析失败 |
| manual_requirements 条目 | 256 (`BS_JSON_MAX_MANUAL_ITEMS`) | 解析失败 |
| JSON 嵌套深度 | 64 (`BS_JSON_MAX_DEPTH`) | 截断 |
| 单字符串长度 | 16384 字节 (`BS_JSON_MAX_STRING_BYTES`) | 截断 |
| 重复 key | 0（不允许） | 解析失败 |
| 根对象额外字段 | 不允许 (`additionalProperties: false`) | 解析失败 |

## 示例：最小配置

```json
{
  "kernel_version": "0.3.0",
  "adapter_version": "0.3.0",
  "instructions": []
}
```

空的 instructions 数组是合法的——表示"不执行任何指令"。

## 示例：完整业务配置

```json
{
  "kernel_version": "0.3.0",
  "adapter_version": "0.3.0",
  "manual_requirements": ["custom_audit"],
  "instructions": [
    {
      "type": "test",
      "name": "approval-chain",
      "metadata": {
        "subject_code": "1001.02",
        "tax_rate": "6",
        "max_amount": "500000",
        "department": "finance",
        "effective_date": "2026-06-01"
      }
    },
    {
      "type": "test",
      "name": "gl-mapping",
      "metadata": {
        "account_code": "10020101",
        "dept_code": "D001",
        "product_code": "P002"
      }
    }
  ]
}
```

## 验证工具

### 使用 Python normalize 脚本

```bash
# 安装依赖
pip install -r tools/normalize/requirements.txt

# 验证配置文件
python tools/normalize/identity_normalize.py \
    --input my_config.json \
    --output /tmp/normalized.json

# 查看输出（如配置无效则抛异常）
cat /tmp/normalized.json
```

### 使用 BlessStar parser（C++ 程序内）

配置字节会被 `ConfigReloadSession` 的 `default_gate` 自动校验。你也可以直接调用 parser：

```cpp
#include <bs/adapter/parser/config_parse.h>

BsConfigParseResult result;
IoReadResult io_result;
io_result.data   = config_bytes;
io_result.length = config_len;

int rc = bs_adapter_parser_parse(io_result, &result, &detail);
if (rc != 0) {
    printf("解析失败: %s\n", detail.buf);
}
bs_adapter_parser_result_destroy(&result);
```

## 与厂商格式的关系

```
厂商文件（JSON/XML/YAML 等）
       │
       ▼  VendorConfigNormalizer（App SDK）
       │
BlessStar Config v1（标准化格式）
       │
       ▼  ConfigReloadSession（App SDK）
       │
       ▼  Parser → Gate Chain → Persist → Kernel
```

厂商格式是输入，Config v1 是内部流通格式。App 开发者应关注 Config v1 的语义，而非厂商格式细节。
