# API 参考

> 参考风格：Spring Cloud API 文档 —— 按模块组织、每个类有完整说明、方法签名与返回值清晰

本文档覆盖 BlessStar App SDK 的所有 public API。SDK 位于 `app/sdk/include/bs/app/sdk/`，共 **10 个头文件**。

---

## 1. AppSession — 应用会话

**头文件**：`<bs/app/sdk/app_session.h>`

RAII 包装器，封装了 BlessStar 运行时初始化流程：`ctx_create` → `bootstrap` → `freeze` → `open_io` → `open_store`。销毁时自动清理。

### 构造与析构

```cpp
explicit AppSession(const char* manifest_path = nullptr);
~AppSession();
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `manifest_path` | `const char*` | 持久化存储 manifest 文件路径。传 `nullptr` 时持久化不可用，但不影响 MEM 配置提交 |

### 移动语义

```cpp
AppSession(AppSession&& other) noexcept;
AppSession& operator=(AppSession&& other) noexcept;
```

AppSession 不可拷贝（`delete`d），但可以移动。

### 访问器

```cpp
bool ok() const;                           // 初始化是否成功
AttachContext* ctx();                      // 获取 AttachContext 指针
const AttachContext* ctx() const;          // const 版本
IoFacade* io();                            // 获取 IoFacade 指针（可为 nullptr）
const IoFacade* io() const;                // const 版本
```

### 使用示例

```cpp
bs::app::AppSession session("/var/bless/manifest.json");
if (!session.ok()) {
    // 处理启动失败
    return;
}
// 使用 ctx() 创建配置提交会话
bs::app::ConfigReloadSession cs(session.ctx());
```

---

## 2. ConfigReloadSession — 配置提交会话

**头文件**：`<bs/app/sdk/config_reload_session.h>`

核心类。用于管理配置的收集、门禁校验和提交。支持 MEM（内存数据）和 FILE（文件数据）双路径提交。

**线程安全**：`ConfigReloadSession` **不是**线程安全的，所有调用必须在创建线程上进行。

### 构造与析构

```cpp
explicit ConfigReloadSession(AttachContext* ctx);
~ConfigReloadSession();
```

`ctx` 来自 `AppSession::ctx()` 或手动创建的 `AttachContext*`。

### 路径添加（数据提交）

```cpp
// 1. MEM 路径：直接提交内存字节（旧接口，完全保留）
bool AddPath(const char* key, const uint8_t* data, size_t len);

// 2. MEM 路径：显式命名的新方法
bool AddMemPath(const char* key, const uint8_t* data, size_t len);

// 3. FILE 路径：提交文件 URI（旧接口，完全保留）
bool AddUri(const char* uri);

// 4. FILE 路径：显式命名的新方法
bool AddFilePath(const char* uri);

// 5. 枚举参数模式：通过 PathSource 指定路径类型
bool AddPath(const char* uri, PathSource source = PathSource::kMem);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `key` | `const char*` | 逻辑 key，用于后续 `GetConfig()` 查询 MEM 数据 |
| `data` | `const uint8_t*` | 配置数据字节（立即 memcpy 复制） |
| `len` | `size_t` | 数据长度 |
| `uri` | `const char*` | 配置 URI（FILE 模式为 `file:///...`） |
| `source` | `PathSource` | 路径来源枚举 |

**`PathSource` 枚举**：

```cpp
enum class PathSource {
    kMem,   // 默认：内存数据
    kFile,  // 文件 URI
    kHttp,  // 预留（未实现，传参会返回 false）
    kHttps  // 预留（未实现，传参会返回 false）
};
```

### 门禁配置

```cpp
// 跳过所有门禁（谨慎使用）
void SetNoGate();

// 添加业务场景策略门禁
void AddPolicyGate(const ScenarioPolicy& policy);
void AddPolicyGates(const std::vector<ScenarioPolicy>& policies);

// 添加自定义门禁函数
void AddCustomGate(
    int (*fn)(const void* data, size_t len, char* err, size_t err_cap, void* ctx),
    void* user_ctx);

// 重置所有门禁配置
void ResetGates();
```

**门禁执行顺序**：`default_gate`（格式解析）→ `policy_gates`（业务场景）→ `custom_gates`（自定义）。

### 执行与查询

```cpp
// 提交所有已添加的路径（同步阻塞）
Report* Commit();

// 取走 Report 所有权（返回后 session 内的 report 指针为 nullptr）
Report* TakeReport();

// 只读访问最近一次 Commit 的 Report（不转交所有权）
const Report* LastReport() const;

// 按逻辑 key 查询配置状态（自动翻译 namespace）
int GetConfig(const char* key, ConfigState* out_state) const;

// 按完整 URI 查询配置状态
int GetConfigByUri(const char* uri, ConfigState* out_state) const;
```

**`Commit()` 执行顺序**：
1. 阶段 1 —— FILE 路径：先持久化（`persist + sync_path`），任一失败整体回滚
2. 阶段 2 —— MEM 路径：FILE 全部成功后执行，跳过持久化，写入审计日志，`sync_path`

### 生命周期管理

```cpp
// 重置 Session 状态（保留门禁配置，清空路径数据）
void Reset();

// 重置门禁配置为初始状态
void ResetGates();
```

### 完整使用模式

```cpp
bs::app::ConfigReloadSession cs(ctx);

// 提交内存 + 文件混合配置
cs.AddMemPath("runtime/feature-flag", flag_data, flag_len);
cs.AddFilePath("file:///etc/bless/baseline.json");

// 设置业务场景门禁
cs.AddPolicyGate({.type = bs::app::ScenarioType::ExpenseReimburse});

// 执行提交
Report* r = cs.Commit();
if (bs_report_get_status(r) == REPORT_STATUS_SUCCESS) {
    // 配置已生效
    
    // 通过逻辑 key 查询 MEM 配置
    ConfigState state;
    cs.GetConfig("runtime/feature-flag", &state);
} else {
    char* json = bs_report_to_json(r);
    // 处理提交失败
    free(json);
}

// 取走 Report 所有权
Report* taken = cs.TakeReport();
if (taken) bs_report_destroy(taken);

// 重置后复用
cs.Reset();
cs.AddMemPath("another/config", data, len);
Report* r2 = cs.Commit();
```

---

## 3. VendorConfigNormalizer — 厂商格式归一化

**头文件**：`<bs/app/sdk/vendor_config_normalizer.h>`

将异构业务配置文件（如业务 JSON）归一化为 BlessStar Config v1 格式。

### 枚举与结构体

```cpp
enum class VendorFormat {
    GenericBusinessJson = 0
    // Yonyou, Kingdee: phase 2 (VP-5)
};

struct NormalizeResult {
    bool                      ok = false;         // 归一化是否成功
    std::vector<std::uint8_t> v1_bytes;           // 归一化后的 v1 字节
    std::string               source_vendor;      // 来源厂商标识
    std::string               scenario;           // 识别出的业务场景
    std::string               error;              // 失败时的错误信息
};
```

### 函数

```cpp
// 读取厂商文件并输出归一化字节
bool NormalizeVendorConfig(VendorFormat fmt,
                           const std::string& vendor_file_path,
                           NormalizeResult* out);
```

| 参数 | 说明 |
|------|------|
| `fmt` | 厂商文件格式。当前支持 `GenericBusinessJson` |
| `vendor_file_path` | 厂商配置文件的路径 |
| `out` | 输出归一化结果（含 v1 字节、场景标识、错误信息） |

### 使用示例

```cpp
bs::app::NormalizeResult result;
if (bs::app::NormalizeVendorConfig(
        bs::app::VendorFormat::GenericBusinessJson,
        "vendor_configs/expense_001.json", &result) && result.ok) {
    // result.v1_bytes 包含标准化的 v1 配置
    cs.AddMemPath("expense/rules", result.v1_bytes.data(), result.v1_bytes.size());
}
```

---

## 4. VendorReloadFacade — 厂商配置重载门面

**头文件**：`<bs/app/sdk/vendor_reload_facade.h>`

提供"厂商文件 → v1 字节 → temp file URI"一站式转换。

### 函数

```cpp
// 厂商文件 → 内存 v1 字节（同 NormalizeVendorConfig）
bool NormalizeFileToV1Bytes(VendorFormat fmt,
                            const std::string& vendor_file_path,
                            NormalizeResult* out);

// 厂商文件 → 写入临时文件 → 返回 file:// URI
bool NormalizeFileToTempUri(VendorFormat fmt,
                            const std::string& vendor_file_path,
                            const std::string& temp_dir,
                            std::string* uri_out,
                            NormalizeResult* out);
```

### 使用示例

```cpp
std::string uri;
bs::app::NormalizeResult result;
if (bs::app::NormalizeFileToTempUri(
        bs::app::VendorFormat::GenericBusinessJson,
        "vendor_configs/expense_001.json",
        "/tmp/bless_vendor", &uri, &result) && result.ok) {
    cs.AddFilePath(uri.c_str());
}
```

---

## 5. AppScenarioPolicy — 场景策略

**头文件**：`<bs/app/sdk/app_scenario_policy.h>`

### 枚举与结构体

```cpp
enum class ScenarioType {
    ExpenseReimburse = 0,   // 费用报销场景
    GlMapping        = 1    // 总账映射场景
};

struct ScenarioPolicy {
    ScenarioType type              = ScenarioType::ExpenseReimburse;
    std::string  tenant;           // 租户标识
    bool         allow_hot_reload  = true;  // 允许热更新
    int          max_batch         = 64;    // 最大批量数
};
```

### 函数

```cpp
// 校验 ScenarioPolicy 是否合法
bool ValidateScenarioPolicy(const ScenarioPolicy& policy);
```

---

## 6. AppVendorPrecheck — 厂商前置检查

**头文件**：`<bs/app/sdk/app_vendor_precheck.h>`

对规范化后的 v1 字节进行最终业务检查。

```cpp
// 对 v1 字节 + ScenarioPolicy 进行业务前置检查
bool PrecheckV1BytesForScenario(const std::uint8_t* data, std::size_t len,
                                const ScenarioPolicy& policy,
                                std::string* error_out);
```

---

## 7. AppIrMapper — 业务 → IR 映射

**头文件**：`<bs/app/sdk/app_ir_mapper.h>`

将 App 层面的业务模型映射为通用 IR 信封。

```cpp
struct AppConfigModel {
    std::string source_vendor;  // 来源厂商
    std::string scenario;       // 业务场景
    std::string uri;            // 配置 URI
    std::string payload;        // 配置载荷
};

struct IrEnvelope {
    std::string uri;
    std::string payload;
};

bool MapToIr(const AppConfigModel& in, IrEnvelope* out);
```

---

## 8. AppContract — 分层调用契约

**头文件**：`<bs/app/sdk/app_contract.h>`

确保 App → Adapter → Kernel 的单向依赖不被破坏。

```cpp
enum class Layer {
    App     = 0,
    Adapter = 1,
    Kernel  = 2
};

// 检查调用方是否允许调用被调方
bool IsCallAllowed(Layer caller, Layer callee);
```

---

## 9. AttachSnapshotUtils — 快照文本转换

**头文件**：`<bs/app/sdk/attach_snapshot_utils.h>`

C-ABI 辅助函数，将 Watch callback 中收到的 snapshot（原始字节）转为可打印字符串。

```c
// 将 snapshot 转为 null-terminated 字符串（需调用 free 释放）
char* bs_attach_watch_snapshot_as_text(const void* snapshot, size_t size);

// 释放 as_text 返回的字符串
void bs_attach_watch_snapshot_text_free(char* text);
```

**使用示例**（在 Watch callback 中）：

```cpp
static void watch_cb(const char* path, ConfigEventType type,
                     const void* snapshot, void* user_data) {
    if (snapshot) {
        // 从测试夹具获取 snapshot 大小（实际需通过其他方式传递）
        char* text = bs_attach_watch_snapshot_as_text(snapshot, snapshot_size);
        if (text) {
            printf("Config changed: %s\n%s\n", path, text);
            bs_attach_watch_snapshot_text_free(text);
        }
    }
}
```

---

## 10. MemAuditLog — MEM 审计日志

**头文件**：`<bs/app/sdk/mem_audit_log.h>`

记录内存配置（`AddMemPath`）提交的审计跟踪。每个 key 保留最近 5 个版本快照。

```cpp
class MemAuditLog {
public:
    static constexpr unsigned kMaxSnapshotsPerKey = 5;

    MemAuditLog();
    ~MemAuditLog();

    // 初始化审计日志目录（创建目录，读取已有 manifest）
    bool Init(const char* audit_dir);

    // 是否已初始化
    bool initialized() const;

    // 记录一个快照（写入 .bin 文件 + 更新 manifest）
    bool Record(const char* key, const void* data, size_t len);

    // 获取最后一条错误信息
    const char* GetLastError() const;
};
```

**`SetAuditDir()` 配置方式（通过 `ConfigReloadSession` 或全局）**：

```cpp
// 在构造 Session 后，Commit 之前设置审计目录
session.SetAuditDir("/var/log/bless/audit");

// 也支持环境变量 BS_AUDIT_DIR（优先级低于 SetAuditDir 调用）
```

未设置审计目录时，MEM 路径可正常提交但不记录审计日志（降级模式）。

---

## 11. ConfigSessionReader — metadata 消费

**头文件**：`<bs/app/sdk/config_session_reader.h>`

配置中的 `instructions.metadata` 字段在 BlessStar 全链路中透明穿通。`ConfigSessionReader` 提供结构化方式消费 metadata，避免 App 开发者手动 JSON 解析。

**线程安全**：内部使用 `std::mutex` 保护所有数据访问，可从 Watch callback 等多线程安全调用。

### 构造

```cpp
// 从 AttachContext* 构造（来自 AppSession::ctx() 或手动创建）
explicit ConfigSessionReader(AttachContext* ctx);
```

与 `ConfigReloadSession` **生命周期独立**——可在 `AppSession` 构造后随时创建，不依赖提交流程。

### 查询 API

```cpp
// 获取指定路径+指令名的 IRInstruction（含 metadata 链）
const IRInstruction* GetInstruction(const char* path_key, const char* instr_name);

// 等价于 GetInstruction(...)?.metadata
const IRMetadata* GetMetadata(const char* path_key, const char* instr_name);

// 手动刷新 gate_cache 引用（自动版本号比对已覆盖大部分场景）
void Refresh();
```

- `path_key`：提交时的完整路径。MEM 路径为 `"mem://<key>"`，FILE 路径为 `"file:///path/to/config"`。
- `instr_name`：`instructions` 数组中指令的 `name` 字段。
- 返回值生命周期由内部 LRU 缓存保活，直到被淘汰或 Refresh 清空。

### 内部缓存机制

| 机制 | 说明 |
|------|------|
| **Gate Cache 复用** | 读取的是 Gate 已校验通过的 `IRInstructionList`，零额外解析开销 |
| **版本号比对** | 每次读取时对比 `hot_update_version`，版本递增时自动刷新缓存 |
| **LRU 淘汰** | 软上限 128 条，超过时淘汰最近最少使用的条目 |
| **深拷贝** | `IRInstruction` 和 `IRMetadata` 链深拷贝到 LRU 缓存，不依赖 Gate Cache 生命周期 |

### 使用示例

**1. 基本的 metadata 消费**

```cpp
#include <bs/app/sdk/config_session_reader.h>

bs::app::ConfigSessionReader reader(session.ctx());

// 假设已提交一个 MEM 配置：AddMemPath("mycfg", data, len)
const IRInstruction* instr = reader.GetInstruction("mem://mycfg", "instruction-name");
if (instr && instr->metadata) {
    // 直接读取结构化 metadata
    const char* subject = bs_ir_instruction_get_metadata(instr, "subject_code");
    const char* rate    = bs_ir_instruction_get_metadata(instr, "tax_rate");
    // ... 无需手动 JSON 解析
}
```

**2. 在 Watch callback 中使用**

```cpp
void MyWatchCallback(const char* path, ConfigEventType type,
                     const void* snapshot, void* user_ctx) {
    auto* reader = static_cast<ConfigSessionReader*>(user_ctx);

    // 线程安全：内部 mutex 保护
    const IRInstruction* instr = reader->GetInstruction(path, "my-instruction");
    if (instr && instr->metadata) {
        const char* val = bs_ir_instruction_get_metadata(instr, "my-meta-key");
        // 消费结构化 metadata ...
    }
}

// 注册 watch
ConfigSessionReader reader(session.ctx());
bs_adapter_attach_config_subscribe_state_watch(
    session.ctx(), "mem://mycfg", MyWatchCallback, &reader);

// 后续配置变更时，Reader 自动检测 hot_update 版本号刷新缓存
```

**3. 热更新后自动刷新**

```cpp
// 第一次读取
const IRInstruction* instr = reader.GetInstruction("mem://mycfg", "vat-rate");
// tax_rate = "13"

// 提交新的配置版本（hot_update）
// ...

// 第二次读取——Reader 自动检测版本号变更，刷新缓存后返回新数据
const IRInstruction* instr2 = reader.GetInstruction("mem://mycfg", "vat-rate");
// tax_rate = "15"（自动更新，无需手动 Refresh）
```

### 生命周期说明

- `GetInstruction` 返回的指针在 LRU 缓存淘汰前有效。
- 显式调用 `Refresh()` 会清空全部 LRU 缓存。
- `ConfigSessionReader` 析构时自动释放所有缓存指令。
- 多个 `ConfigSessionReader` 实例可以共享同一个 `AttachContext`，各自有独立 LRU 缓存。

---

## 附录：API 速查表

| 头文件 | 关键类/函数 | 用途 |
|--------|------------|------|
| `app_session.h` | `AppSession` | RAII 运行时启动 |
| `config_reload_session.h` | `ConfigReloadSession` | 配置提交流程 |
| `vendor_config_normalizer.h` | `NormalizeVendorConfig` | 厂商格式归一化 |
| `vendor_reload_facade.h` | `NormalizeFileToTempUri` | 归一化→文件 URI |
| `app_scenario_policy.h` | `ScenarioPolicy` | 业务场景策略 |
| `app_vendor_precheck.h` | `PrecheckV1BytesForScenario` | 前置业务检查 |
| `app_ir_mapper.h` | `MapToIr` | 业务→IR 映射 |
| `app_contract.h` | `IsCallAllowed` | 分层调用检查 |
| `attach_snapshot_utils.h` | `snapshot_as_text` | 快照转文本 |
| `mem_audit_log.h` | `MemAuditLog` | MEM 审计日志 |
| `config_session_reader.h` | `ConfigSessionReader` | metadata 结构化消费 |
