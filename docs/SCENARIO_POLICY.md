# 场景策略与门禁

> 参考风格：Spring Cloud 配置文档 —— 策略说明 + 配置选项 + 示例

## 概述

场景策略（`ScenarioPolicy`）是 BlessStar 的**业务级门禁机制**。在配置通过 `default_gate`（格式校验）后，BlessStar 会使用场景策略对配置进行业务语义层面的验证，确保：

- 配置内容符合预期业务场景
- 租户隔离生效
- 热更新行为可控
- 批量提交不超限

## ScenarioPolicy

### 定义

```cpp
enum class ScenarioType {
    ExpenseReimburse = 0,   // 费用报销
    GlMapping        = 1    // 总账映射
};

struct ScenarioPolicy {
    ScenarioType type              = ScenarioType::ExpenseReimburse;
    std::string  tenant;           // 租户标识
    bool         allow_hot_reload  = true;   // 允许热更新
    int          max_batch         = 64;     // 最大批量数
};
```

### 字段说明

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `type` | `ScenarioType` | `ExpenseReimburse` | 业务场景类型，决定场景校验规则 |
| `tenant` | `string` | `""`（空字符串） | 租户标识。用于多租户场景下的配置隔离 |
| `allow_hot_reload` | `bool` | `true` | 是否允许热更新。设为 `false` 时，已存在的配置不会被覆盖 |
| `max_batch` | `int` | `64` | 单次批量提交的最大指令数。超过时报错 |

### 使用示例

```cpp
bs::app::ConfigReloadSession cs(ctx);

// 设置费用报销场景策略
bs::app::ScenarioPolicy policy;
policy.type             = bs::app::ScenarioType::ExpenseReimburse;
policy.tenant           = "tenant-a";
policy.allow_hot_reload = true;
policy.max_batch        = 128;

cs.AddPolicyGate(policy);
```

## 门禁链执行顺序

一个配置在被提交到 Kernel 之前，会依次通过三个门禁阶段：

```
               ┌───────────────────────────────────┐
               │   default_gate（第一阶段）           │
               │   - JSON 格式校验                   │
               │   - IR 结构解析                     │
               │   - 安全审计（UTF-8、深度、重复key）  │
               └──────────────┬────────────────────┘
                              ▼
               ┌───────────────────────────────────┐
               │   policy_gates（第二阶段）           │
               │   - ScenarioPolicy 业务校验         │
               │   - 租户隔离检查                    │
               │   - 热更新权限检查                   │
               │   - 批量上限检查                     │
               └──────────────┬────────────────────┘
                              ▼
               ┌───────────────────────────────────┐
               │   custom_gates（第三阶段）           │
               │   - 用户自定义校验函数               │
               │   - 任意业务逻辑检查                 │
               └──────────────┬────────────────────┘
                              ▼
                         Kernel (ConfigManager)
```

### default_gate（内置）

`default_gate` 是 BlessStar 内置的格式化门禁，对所有配置自动执行：

- 解析 JSON 为指令结构
- 检查必需字段（`kernel_version`, `adapter_version`, `instructions`）
- 校验安全限制（UTF-8 / 嵌套深度 / 重复 key / 字符串长度 / 指令数量）
- 缓存解析结果供后续门禁复用（APP-PUSH-4）

默认情况下，`default_gate` 总是启用。要完全跳过它，需调用 `SetNoGate()`。

### policy_gates（业务策略门禁）

通过 `AddPolicyGate()` / `AddPolicyGates()` 添加的场景策略门禁：

- 每一条 `ScenarioPolicy` 对应一个校验规则
- 多个 policy 可以同名添加（`AddPolicyGates({p1, p2})`）
- 所有 policy 全部通过才能进入下一阶段

### custom_gates（自定义门禁）

通过 `AddCustomGate()` 添加自定义校验函数：

```cpp
int my_gate_fn(const void* data, size_t len, char* err, size_t err_cap, void* ctx) {
    // 自定义校验逻辑
    // 返回 0 = 通过，非零 = 拒绝（错误信息写入 err）
    return 0;
}

cs.AddCustomGate(my_gate_fn, nullptr);     // 无 user_ctx
cs.AddCustomGate(my_gate_fn, my_state);    // 带 user_ctx
```

自定义 gate 的签名：

```cpp
int (*gate_fn)(const void* data, size_t len, char* err, size_t err_cap, void* ctx);
```

| 参数 | 说明 |
|------|------|
| `data` | 配置数据字节 |
| `len` | 数据长度 |
| `err` | 错误缓冲区（gate 拒绝时写入错误信息） |
| `err_cap` | 错误缓冲区大小 |
| `ctx` | 注册时传入的 `user_ctx` |
| **返回值** | 0 = 通过；非零 = 拒绝 |

## 场景类型详解

### ExpenseReimburse（费用报销）

用于费用报销审批链的配置校验：

- 校验审批链配置的完整性
- 校验金额、税率的格式
- 检查必要的 metadata 字段是否存在

### GlMapping（总账映射）

用于总账科目映射的配置校验：

- 校验科目代码格式
- 校验部门/产品代码的合规性
- 检查映射关系是否完整

## 门禁配置的组合模式

### 无门禁（仅测试/调试）

```cpp
session.SetNoGate();  // 跳过所有门禁
```

### 单一业务场景

```cpp
session.AddPolicyGate({
    .type = bs::app::ScenarioType::ExpenseReimburse,
    .tenant = "tenant-a"
});
```

### 多重策略

```cpp
std::vector<bs::app::ScenarioPolicy> policies = {
    {.type = bs::app::ScenarioType::ExpenseReimburse, .tenant = "tenant-a"},
    {.type = bs::app::ScenarioType::ExpenseReimburse, .tenant = "tenant-b"},
};
session.AddPolicyGates(policies);
```

### 策略 + 自定义门禁

```cpp
session.AddPolicyGate(expense_policy);
session.AddCustomGate(encryption_check_fn, nullptr);
session.AddCustomGate(compliance_audit_fn, audit_state);
```

### 重置门禁

```cpp
session.ResetGates();  // 清空所有 policy_gates 和 custom_gates
```

## 常见问题

**Q: `SetNoGate()` 和 `ResetGates()` 的区别？**
A: `SetNoGate()` 设置一个跳过标志，在本次 Commit 中绕过所有门禁（包括 `default_gate`）。`ResetGates()` 清空已添加的 `policy_gates` 和 `custom_gates`，但保留 `default_gate` 生效。

**Q: `AddPolicyGate()` 多次调用和 `AddPolicyGates({})` 的区别？**
A: 功能等价。多次调用 `AddPolicyGate(p)` 等价于一次性传入 vector。

**Q: 如果所有 policy_gates 都为空数组，会怎样？**
A: 相当于没有业务策略门禁，`default_gate` 通过后直接进入 Kernel。
