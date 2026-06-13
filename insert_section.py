import sys
sys.stdout.reconfigure(encoding='utf-8')

new_section = r'''
## 第24天/方案E — Policy Gate 声明式 metadata 校验 + CustomGate 工具函数（2026-06-13 新增）

> **状态**：🟢 **Policy Gate 声明式 metadata 校验（方案 E）** 已全部用户裁定，待子任务B工程落地。

---

### 问题背景

App 层 `ScenarioPolicy` 的 `PrecheckV1BytesForScenario()` 当前只对原始 JSON 字节做 string 级检查（`find("\"kernel_version\"")`），无法对 `instructions.metadata` 做结构化校验。

同时 `session_gate_fn()` 的 `default_gate`（Step 1）已解析出 `IRInstructionList*`，但 Step 2（policy）和 Step 3（custom）均未收到，违反了 **APP-PUSH-4** 的缓存共享承诺。

**核心短板**：
- Policy gate 无法声明式地校验 metadata 中某个 key 的值是否等于/大于/小于某个值
- Custom gate 如果想校验 metadata，必须自行重新解析 JSON（二次解析）

---

### 方案演进历程

| 方案版本 | 核心思想 | 优点 | 缺点 | 状态 |
|---------|---------|------|------|------|
| 方案 A — 透传 parsed 指针 + `MetaRule` 嵌入 ScenarioPolicy | `PrecheckV1BytesForScenario` 重载接收 `IRInstructionList*`，内部遍历匹配声明式规则 | 声明式、非 breaking、零额外解析 | 规则集固定、仅限 App SDK 用 | 📋 备选 |
| 方案 B — `CustomGate` 签名扩展 | 扩展 `CustomGateEntry` 回调签名含 `IRInstructionList*` | 最灵活、任意结构化校验 | **Breaking change**、用户需改回调签名 | 📋 备选 |
| **方案 C — 声明式 Policy + 底层工具函数库（分层）** | 底层 `bs_parser_meta_rule_check()` 纯 C 函数 + 上层 `ScenarioPolicy.metadata_rules` 声明式 | Policy 声明式 + CustomGate 可用底层函数、非 breaking、分层复用 | 实现量最大（~120 行） | ⭐ **用户已确认采用** |

---

### 最终方案架构

```
┌─────────────────────────────────────────────────────────────┐
│                     App SDK                                   │
│  ScenarioPolicy + MetaRule[]（声明式规则）                      │
│  → PrecheckV1BytesForScenario(data, len, instructions, policy)│
│  → 复用 default_gate 的 parse_result（零额外解析）              │
│  → 调用底层 bs_parser_meta_rule_check()                      │
└──────────────┬──────────────────────────────────────────────┘
               │ IRInstructionList* 透传
┌──────────────▼──────────────────────────────────────────────┐
│           adapter/parser（底层纯 C 函数）                       │
│  bs_parser_meta_rule_check(instructions, rules, rule_count) │
│  → 10 个操作符：eq/ne/gt/lt/ge/le/exists/not_exists/        │
│     regex/contains                                          │
│  → 按 instr_name 精确匹配或全匹配（空=匹配所有）                │
└──────────────┬──────────────────────────────────────────────┘
               │ 可被 CustomGate 内部调用
┌──────────────▼──────────────────────────────────────────────┐
│  CustomGate（函数式检测）                                      │
│  → 用户自行 parse 后调用 bs_parser_meta_rule_check()          │
│  → 或做脚本调用、复杂运算等                                    │
└─────────────────────────────────────────────────────────────┘
```

---

### 核心设计要点

#### 1️⃣ `BsMetaRule` 结构体（底层 C 层）

```c
// adapter/parser/meta_rule.h
typedef enum {
    BS_META_EQ = 0,           // ==
    BS_META_NE,               // !=
    BS_META_GT,               // >
    BS_META_LT,               // <
    BS_META_GE,               // >=
    BS_META_LE,               // <=
    BS_META_EXISTS,           // key 存在
    BS_META_NOT_EXISTS,       // key 不存在
    BS_META_REGEX,            // 正则匹配
    BS_META_CONTAINS          // 包含子串
} BsMetaOp;

typedef struct {
    const char* instr_name;   // NULL 或 "" = 匹配所有指令
    const char* key;          // metadata 字段名
    BsMetaOp    op;           // 操作符
    const char* value;        // 比较值（op=exists/not_exists 时忽略）
} BsMetaRule;
```

#### 2️⃣ `bs_parser_meta_rule_check()` 底层函数

```c
// adapter/parser/meta_rule.h
// 全部规则通过返回 (size_t)-1
// 失败时返回第一条不匹配的规则索引（0-based）
// err/err_cap 填充第一条失败规则的错误描述
size_t bs_parser_meta_rule_check(
    const IRInstructionList* instructions,
    const BsMetaRule* rules,
    size_t rule_count,
    char* err, size_t err_cap);
```

#### 3️⃣ `MetaRule` 声明式结构（App SDK C++ 层）

```cpp
// app/sdk/include/bs/app/sdk/app_meta_rule.h
namespace bs::app {

struct MetaRule {
    std::string instr_name;   // "" = 匹配所有指令
    std::string key;          // metadata 字段名
    BsMetaOp    op = BS_META_EQ;
    std::string value;        // 比较值
};

// 转换为 C 层 BsMetaRule（不持有底层指针）
void to_c_rule(const MetaRule& src, BsMetaRule* dst);

} // namespace bs::app
```

#### 4️⃣ `ScenarioPolicy` 扩展

```cpp
// app/sdk/include/bs/app/sdk/app_scenario_policy.h
struct ScenarioPolicy {
    ScenarioType type = ScenarioType::ExpenseReimburse;
    std::string  tenant;
    bool         allow_hot_reload = true;
    int          max_batch        = 64;
    std::vector<MetaRule> metadata_rules;  // 新增（可选）
};
```

#### 5️⃣ `PrecheckV1InstructionsForScenario()` 透传函数

```cpp
// app/sdk/include/bs/app/sdk/app_vendor_precheck.h
// 新增重载：接收已解析的 IRInstructionList*
bool PrecheckV1InstructionsForScenario(
    const uint8_t* data, size_t len,
    const IRInstructionList* instructions,   // default_gate 已解析
    const ScenarioPolicy& policy,
    std::string* error_out);
```

#### 6️⃣ `session_gate_fn` 改动

```cpp
// app/sdk/src/config_reload_session.cpp
// Step 2 改为调用新重载
for (const auto& policy : *ctx->policy_gates) {
    std::string err;
    if (!PrecheckV1InstructionsForScenario(
            read_result->data, read_result->length,
            parse_result.instructions,   // ← APP-PUSH-4 透传
            policy, &err)) {
        if (detail_out)
            std::snprintf(detail_out->buf, sizeof(detail_out->buf),
                          "%s", err.c_str());
        bs_adapter_parser_result_destroy(&parse_result);
        return BS_RELOAD_GATE_IR_REJECT;
    }
}
```

---

### 用户确认的关键决策

| 选择项 | 用户选择 | 说明 |
|-------|---------|------|
| 方案选择 | **C — 声明式 Policy + 底层工具函数库** | 分层：Policy 声明式 + CustomGate 可用底层函数 |
| 透传方式 | **新增 `PrecheckV1InstructionsForScenario`** | 新重载接收 `IRInstructionList*`，不改旧函数 |
| 操作符集 | **10 个（最终集）** | eq/ne/gt/lt/ge/le/exists/not_exists/regex/contains |
| 底层函数放置层 | **adapter/parser/** | `bs_parser_meta_rule_check()` |
| `instr_name` 空值语义 | **空 = 匹配所有指令** | 遍历 `IRInstructionList` 每条指令 |

---

### 落地建议（子任务B待执行）

#### 落地项总表

| 落地项 | 状态 | 影响范围 / 交付物 | 对应裁定点 |
|-------|------|-----------------|-----------|
| MD-E-01 — `BsMetaRule` + `BsMetaOp` 枚举 + `bs_parser_meta_rule_check()` | 🟪 | `adapter/parser/include/bs/adapter/parser/meta_rule.h` + `adapter/parser/src/meta_rule.c` | 核心 |
| MD-E-02 — `MetaRule` C++ 结构 + `to_c_rule()` 转换 | 🟪 | `app/sdk/include/bs/app/sdk/app_meta_rule.h` + `app/sdk/src/app_meta_rule.cpp` | 声明式 |
| MD-E-03 — `ScenarioPolicy` 新增 `metadata_rules` 字段 | 🟪 | `app/sdk/include/bs/app/sdk/app_scenario_policy.h` | 声明式 |
| MD-E-04 — `PrecheckV1InstructionsForScenario` 新重载 | 🟪 | `app/sdk/include/bs/app/sdk/app_vendor_precheck.h` + `app/sdk/src/app_vendor_precheck.cpp` | 透传 |
| MD-E-05 — `session_gate_fn` 透传 `parse_result.instructions` | 🟪 | `app/sdk/src/config_reload_session.cpp`（Step 2 改为新重载） | APP-PUSH-4 修复 |
| MD-E-06 — CMake：parser 源文件 + SDK 源文件加入构建 | 🟪 | `CMakeLists.txt` | 编译 |
| MD-E-07 — 全链路测试（metadata 校验场景） | 🟪 | `BsRealBizFullChainTest.cpp` Scenario 9+ | 全 |
| MD-E-08 — 更新项目修改记录.md | 🟪 | `项目修改记录.md` | — |

#### 实现要点（子任务B必读）

1. **MD-E-01（底层匹配引擎）**：
   - `bs_parser_meta_rule_check()` 遍历 `IRInstructionList`，对每条 `IRInstruction` 匹配 `BsMetaRule` 指定的 `instr_name`
   - `instr_name` 为空/NULL 时匹配所有指令
   - 数值比较（gt/lt/ge/le）：metadata value 用 `strtoll` 转 long long 比较
   - `regex`：使用标准 `<regex>` 或简易匹配器（MVP 可用 `strstr` 子串匹配 + 框架预留 regex）
   - `contains`：metadata value 中包含 rule value 子串
   - 错误消息格式：`"rule[%zu]: instr='%s' key='%s' op=%s value='%s' failed: %s"`

2. **MD-E-05（APP-PUSH-4 修复）**：
   - `session_gate_fn` Step 2 从 `PrecheckV1BytesForScenario(...)` 改为 `PrecheckV1InstructionsForScenario(..., parse_result.instructions, ...)`
   - `PrecheckV1BytesForScenario` 保持原签名不变（向后兼容）

#### 验收标准（主任务审核用）

| 项 | 标准 |
|----|------|
| MD-E-01 | `bs_parser_meta_rule_check()` 10 个操作符均有测试用例 |
| MD-E-02 | `MetaRule` ↔ `BsMetaRule` 转换无损 |
| MD-E-03 | `ScenarioPolicy` 带 `metadata_rules` 构造正常，不带时默认空（向后兼容） |
| MD-E-04 | `PrecheckV1InstructionsForScenario` + `PrecheckV1BytesForScenario` 并存 |
| MD-E-05 | `session_gate_fn` 透传后 policy gate 可校验结构化 metadata |
| MD-E-07 | 新增 Scenario 覆盖：eq/gt/lt/exists/not_exists/regex/contains + 全匹配 + 单指令匹配 |

---

### 方案解决的短板

| 短板 ID | 问题 | 方案 E 解决方式 |
|--------|------|---------------|
| POLICY-META-01 | `parse_result` 在 policy/custom gate 中不可用（APP-PUSH-4 未兑现） | `PrecheckV1InstructionsForScenario` 透传 `IRInstructionList*` |
| POLICY-META-02 | Policy gate 无法声明式校验 metadata | `MetaRule` 结构 + `metadata_rules` 字段 |
| POLICY-META-03 | Custom gate 二次解析 JSON | `bs_parser_meta_rule_check()` 纯 C 底层函数，CustomGate 可复用 |

---

### 方案状态

- **专题确认时间**：2026-06-13
- **子任务 A**：✅ 研析完成（迭代至方案 C）
- **子任务 B**：🟪 待执行
- **状态**：🟡 进行中（架构已闭合；工程未落地）

---

### 刻意未定义项（Policy Gate metadata 校验 MVP）

| 未定义项 | 说明 |
|---------|------|
| `regex` 操作符的正则引擎选择 | 首版建议用标准库 `<regex>` 或 `strstr` 子串匹配框架预留；二期换 PCRE |
| metadata value 的数值比较精度 | 首版用 `strtoll`（整数比较）；浮点值的比较留待二期 |
| `MetadataRule` 跨 Policy 复用 | 首版每条 `ScenarioPolicy` 独立拥有 `metadata_rules`；二期可考虑提取 `MetadataRuleSet` 共享对象 |
| `CustomGate` 接收 `IRInstructionList*` | 首版不改 `CustomGate` 签名；用户可通过底层函数自行 parse 后调用 |

---

### 架构短板登记（第24天 · Policy Gate metadata 校验）

| ID | 短板主题 | 架构影响（简述） | 建议补齐时机 | 状态 | 参见 |
|----|---------|---------------|-----------|------|------|
| POLICY-META-01 | `parse_result` 在 policy/custom gate 中不可用 | APP-PUSH-4 缓存共享承诺未兑现 | 本专题 | 🟪 待补齐 | 方案 E MD-E-05 |
| POLICY-META-02 | Policy gate 无法声明式校验 metadata | App 开发者需手写 CustomGate | 本专题 | 🟪 待补齐 | 方案 E MD-E-01～04 |
| POLICY-META-03 | Custom gate 二次解析 JSON | 性能浪费 + 代码冗余 | 本专题 | 🟪 待补齐 | 方案 E MD-E-01 |

---

### 轮次元信息（第24天 · Policy Gate metadata 校验索引）

| 字段 | 值 |
|------|-----|
| 轮次 | 第24天 · **Policy Gate 声明式 metadata 校验（方案 E · MD-E-01～08）** |
| 用户已裁决 | 方案C，透传=新增PrecheckV1InstructionsForScenario，操作符=10个最终集，底层函数=adapter/parser/bs_parser_meta_rule_check()，instr_name空=全匹配 |
| 未决 | —（全部裁定完毕） |
| 下一步 | 下发子任务B 工程落地 → 落地后主任务审核 → 进入下一天 |
'''

with open('架构方案选择记录.md', 'r', encoding='utf-8') as f:
    content = f.read()

insert_before = '## 跨日汇总索引（文档级）'
if insert_before in content:
    idx = content.find(insert_before)
    new_content = content[:idx] + new_section + '\n' + content[idx:]
    with open('架构方案选择记录.md', 'w', encoding='utf-8') as f:
        f.write(new_content)
    print('OK: inserted new section, total length =', len(new_content))
else:
    print('ERROR: marker not found')
