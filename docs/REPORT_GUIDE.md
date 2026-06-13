# Report 解读指南

> 参考风格：Linux 错误码手册 —— 分类明确，每个错误码有原因和修复建议

## 什么是 Report？

每次 `Commit()` 调用都会生成一份 **Report**，记录本次配置提交的执行结果。Report 是判断配置是否生效的唯一权威来源。

### 快速速查

```cpp
Report* r = cs.Commit();

// 检查总体状态
if (bs_report_get_status(r) == REPORT_STATUS_SUCCESS) {
    // ✅ 配置已生效
} else {
    // ❌ 配置提交失败
}

// 查看详细错误
char* json = bs_report_to_json(r);
printf("%s\n", json);
free(json);
```

## Report 结构

### 总体状态

| 状态值 | 含义 |
|--------|------|
| `REPORT_STATUS_SUCCESS` | 配置已成功提交并生效 |
| `REPORT_STATUS_FAILED` | 配置提交失败（见错误列表了解原因） |
| `REPORT_STATUS_PENDING` | 提交尚未完成（内部状态，通常不会暴露给用户） |

### Report JSON 输出

调用 `bs_report_to_json()` 可获得 Report 的完整 JSON 表示：

```json
{
  "config_reload_session": {
    "start": "2026-06-12T12:00:00Z",
    "end": "2026-06-12T12:00:00.050Z",
    "status": "FAILED",
    "errors": [
      {
        "category": "parse",
        "message": "JSON parse error at line 3: unexpected token"
      }
    ]
  }
}
```

## 错误分类

Report 中的错误按来源分类：

### 1. Session 层错误 `[session]`

| 错误消息 | 原因 | 修复 |
|----------|------|------|
| `null attach context` | `ConfigReloadSession` 用空指针构造 | 检查 `AppSession::ok()` 和 `AppSession::ctx()` |
| `no paths added` | Commit 前没有添加任何路径 | 确保调用了 `AddMemPath()`/`AddFilePath()` |
| `failed to create batch controller` | 内部资源分配失败 | 检查内存使用情况 |
| `no read fn configured` | FILE 路径未设置 read_fn 且 IoFacade 不可用 | 确保 `open_io` 已调用，或设置 `SetReadFn()` |

### 2. 解析错误 `[parse]`

| 错误消息 | 原因 | 修复 |
|----------|------|------|
| `JSON parse error at line N: ...` | 配置 JSON 格式不合法 | 使用 JSON 校验工具检查配置 |
| `missing required field: kernel_version` | 根对象缺少必需字段 | 参考 [CONFIG_FORMAT.md](CONFIG_FORMAT.md) 补全字段 |
| `unknown field: xxx` | 根对象包含未定义字段 | 移除 `additionalProperties` 之外的字段 |
| `too many instructions (2048 max)` | 指令数量超限 | 拆分配置或减少指令数 |
| `duplicate key: xxx` | 对象中出现重复 key | 删除重复 key |
| `UTF-8 encoding error` | 配置包含非法 UTF-8 序列 | 确保配置为合法 UTF-8 编码 |

### 3. 门禁拒绝 `[ir_gate]`

| 错误消息 | 原因 | 修复 |
|----------|------|------|
| `empty read result` | 读取到的数据为空 | 检查配置来源是否为空 |
| `scenario policy mismatch` | 配置内容与场景策略不匹配 | 检查 `ScenarioPolicy` 配置 |
| `hot-reload not allowed` | 配置已存在且 `allow_hot_reload = false` | 设置 `allow_hot_reload = true` 或先清理旧配置 |
| `max_batch exceeded` | 单次提交指令数超过 `max_batch` | 增大 `max_batch` 或减少指令数 |
| `custom gate rejected: ...` | 自定义门禁拒绝 | 根据自定义 gate 的错误信息处理 |

### 4. 持久化错误 `[persist]`

| 错误消息 | 原因 | 修复 |
|----------|------|------|
| `store not opened` | 持久化存储未初始化 | 调用 `bs_adapter_attach_ctx_open_persist_store()` |
| `commit conflict` | revision 冲突，配置已被其他进程修改 | 重新读取后提交 |
| `IO error: ...` | 文件写入失败 | 检查磁盘空间、权限 |
| `WAL write failed` | 预写日志写入失败 | 检查文件系统状态 |

## LastReport() 使用

`ConfigReloadSession::LastReport()` 提供对最近一次 Commit 结果的只读访问：

```cpp
Report* r = cs.Commit();

// 后续可以在不取走所有权的情况下检查结果
const Report* last = cs.LastReport();
if (last) {
    printf("上次提交状态: %d\n", bs_report_get_status(last));
}

// 取走所有权后，LastReport() 返回 nullptr
Report* taken = cs.TakeReport();
assert(cs.LastReport() == nullptr);
```

## 常见排查路径

### 场景 1：Commit 返回 FAILED，但不知道原因

```cpp
Report* r = cs.Commit();
if (bs_report_get_status(r) == REPORT_STATUS_FAILED) {
    char* json = bs_report_to_json(r);
    fprintf(stderr, "提交失败，完整报告:\n%s\n", json);
    free(json);
    
    // 检查各阶段错误
    // 错误分类前缀：session, parse, ir_gate, persist
}
```

### 场景 2：Commit 成功（SUCCESS），但配置未生效

```cpp
// 使用 GetConfig 检查配置状态
ConfigState state;
int rc = cs.GetConfig("my-key", &state);
if (rc == 0 && state == CONFIG_STATE_ACTIVE) {
    printf("配置已生效!\n");
} else {
    printf("配置状态: %d (错误码: %d)\n", state, rc);
}
```

### 场景 3：热更新后配置未更新

检查：
1. `ScenarioPolicy.allow_hot_reload` 是否为 `true`
2. 新配置的 `instructions` 内容是否与旧配置不同（如果完全相同，Kernel 可能不做更新）
3. 使用 `GetConfig()` 对比新旧版本内容

### 场景 4：MEM 配置提交成功但重启后丢失

这是预期行为——MEM 配置**不持久化到磁盘**。如需持久化，请使用 `AddFilePath()` 或者配置 `SetAuditDir()` 开启审计日志。

## Report 生命周期

```
Commit()
  │
  ├─ session.report_ = bs_report_create()
  ├─ 设置状态为 SUCCESS
  ├─ 执行各个阶段
  │   ├─ 失败 → 设置状态为 FAILED + 添加错误
  │   └─ 成功 → 保持 SUCCESS
  ├─ 返回 report_（所有权在 session 中）
  │
  ▼  是否取走？
  │
  ├─ TakeReport() → 所有权转移给调用方
  │   └─ 调用方负责 bs_report_destroy()
  │
  ├─ 不取走 + 再次 Commit()
  │   └─ 旧 Report 自动销毁
  │
  └─ 不取走 + Session 析构
      └─ 旧 Report 自动销毁
```

**最佳实践**：除非你需要在 Commit 后长时间持有 Report，否则可以使用 `LastReport()` 只读访问，让 Session 管理生命周期。
