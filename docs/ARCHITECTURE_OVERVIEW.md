# BlessStar 架构总览

> 参考：Spring Cloud 分层解耦模式、Kubelet 配置驱动生命周期、ZooKeeper Watcher 事件模型

## 设计哲学

BlessStar 的设计借鉴了 Kubelet 的"配置即状态"思想、Spring Cloud 的分层解耦模式、以及 ZooKeeper 的 Watcher 事件驱动模型，但针对**财务运维配置管理**这个特定领域做了大量精简和增强。

### 核心原则

- **配置即指令**：配置不是静态文件，而是由内核流水线执行的指令（IR）
- **单向依赖**：App → Adapter → Kernel，禁止反向和横向依赖
- **门禁即契约**：配置进入内核前必须通过格式、策略、自定义三级校验
- **原子性承诺**：要么全部生效，要么全部回滚，不存在半提交状态
- **可审计**：每次配置变更都有记录，支持回溯和追踪

## 三层架构

```
┌──────────────────────────────────────────────────────────────┐
│                        App Layer (SDK)                        │
│                                                              │
│  ┌──────────────┐  ┌──────────────────┐  ┌───────────────┐  │
│  │  AppSession   │  │ConfigReloadSession│  │  MemAuditLog  │  │
│  │  RAII 一行启动 │  │  MEM/FILE 双路径   │  │  审计日志 N=5 │  │
│  └──────┬───────┘  └────────┬─────────┘  └───────┬───────┘  │
│  ┌──────┴───────┐  ┌────────┴─────────┐  ┌───────┴───────┐  │
│  │VendorNormalize│  │ ScenarioPolicy   │  │  AppContract  │  │
│  │ 格式归一化     │  │ 场景策略门禁     │  │  分层调用契约  │  │
│  └──────────────┘  └──────────────────┘  └───────────────┘  │
│                                                              │
│  App SDK 职责：① 业务配置归一化 ② 策略验证 ③ 配置提交流程封装  │
└──────────────────────────┬───────────────────────────────────┘
                           │ 标准化 IR + Report
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                      Adapter Layer (编排)                     │
│                                                              │
│  ┌──────────────────┐  ┌──────────────────────────────┐     │
│  │  Gate Chain       │  │  ReloadBatchController       │     │
│  │  ┌─default_gate  │  │  ┌─PER_PATH / PER_BATCH     │     │
│  │  ├─policy_gates  │  │  ├─persist + sync_path      │     │
│  │  └─custom_gates  │  │  └─两阶段提交 (FILE→MEM)    │     │
│  └──────────────────┘  └───────────┬──────────────────┘     │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐     │
│  │  Parser      │  │  WAL + Store │  │  Watch        │     │
│  │  JSON→AST→IR │  │  原子持久化   │  │  状态变更通知  │     │
│  └──────────────┘  └──────────────┘  └───────────────┘     │
│                                                              │
│  Adapter 职责：① 配置读取与解析 ② 门禁校验 ③ 原子持久化        │
│               ④ 内核状态同步 ⑤ 状态变更事件通知               │
└──────────────────────────┬───────────────────────────────────┘
                           │ 纯净 IR
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                       Kernel Layer (内核)                     │
│                                                              │
│  ┌────────────┐  ┌────────────┐  ┌────────────────────┐    │
│  │ ConfigMgr  │  │  Pipeline  │  │  Executor Pool     │    │
│  │  配置状态机  │  │  流水线指令  │  │  并发执行器池       │    │
│  │  ACTIVE     │  │  execute   │  │  steady=3, max=10 │    │
│  │  STAGING    │  │  validate  │  │                    │    │
│  │  OBSOLETE   │  │  commit    │  │                    │    │
│  └──────┬─────┘  └────────────┘  └────────────────────┘    │
│  ┌──────┴─────┐  ┌────────────┐  ┌────────────────────┐    │
│  │  StateBus  │  │  EventBus  │  │  Registry + IO     │    │
│  │  状态分片总线│  │  事件发布/   │  │  注册中心 + IO 抽象│    │
│  │            │  │  订阅      │  │  层                │    │
│  └────────────┘  └────────────┘  └────────────────────┘    │
│                                                              │
│  Kernel 职责：① 配置全生命周期管理 ② 指令流水线执行           │
│              ③ 状态与事件管理 ④ 并发执行池                    │
└──────────────────────────────────────────────────────────────┘
```

## 核心概念

### IR（Intermediate Representation）

IR 是 Adapter 输出给 Kernel 的标准化指令格式。一条 IR 指令包含：

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 指令类型（如 `"test"`），由 Kernel builtin + merge_activation 决定允许范围 |
| `name` | string | 指令名称，业务逻辑标识 |
| `metadata` | map<string,string> | 指令参数键值对 |

示例：

```json
{
  "type": "test",
  "name": "approval-chain-v1",
  "metadata": {
    "subject_code": "1001.02",
    "tax_rate": "10",
    "max_amount": "500000"
  }
}
```

### Config 状态机

每个配置在其生命周期中经过以下状态：

```
LOADING ──→ ACTIVE ←── hot_update
  │            │
  └──→ FAILED  └──→ OBSOLETE
```

| 状态 | 说明 |
|------|------|
| `LOADING` | 配置正在加载中 |
| `ACTIVE` | 配置已生效，可被查询和使用 |
| `STAGING` | 暂存状态（PER_BATCH 中间态） |
| `FAILED` | 加载失败（格式错误/门禁拒绝） |
| `OBSOLETE` | 已被新版本替代 |

### Gate Chain（门禁链）

所有配置在提交到 Kernel 之前，必须通过三层门禁：

```
配置字节 → default_gate ──→ policy_gates ──→ custom_gates ──→ Kernel
               │                │                 │
           格式校验          业务策略校验        自定义逻辑
         JSON 解析检查      场景+租户规则       用户注入函数
```

### Report（执行报告）

每次 `Commit()` 产生一份 `Report`，包含：

- **状态**：`SUCCESS` / `FAILED`
- **错误列表**：按阶段分类的错误详情（`[session]`、`[parse]`、`[ir_gate]`、`[persist]`等）
- **时间戳**：开始和结束时间
- **JSON 序列化**：Report 可转为 JSON 字符串用于日志和诊断

### Watch（状态变更通知）

Kernel 支持对配置状态变更进行订阅。当配置发生 `ACTIVE`/`FAILED`/`OBSOLETE` 转换时，注册的 watch callback 被调用，携带配置路径和快照数据（原始字节）。

## 配置提交流程详解

以下是一次完整的配置提交流程（以 `AddFilePath` 为例）：

```
App 代码
  │
  ├─ AppSession(ctx_create→bootstrap→freeze→open_io→open_store)
  │
  ├─ ConfigReloadSession.AddFilePath("file:///etc/bless/expense.json")
  │   └─ pending_uris_.push_back("file:///...")
  │
  ├─ ConfigReloadSession.AddPolicyGate(scenario_policy)
  │
  ├─ ConfigReloadSession.Commit()
  │   │
  │   ├── [阶段 1: FILE 路径 ── 先持久化]
  │   │   ├─ IoFacade 读取文件内容
  │   │   ├─ Gate Chain 校验（格式→策略→自定义）
  │   │   ├─ Persist Store: write_file_atomic（WAL + CAS）
  │   │   ├─ sync_path: ConfigManager.load_config() → ACTIVE
  │   │   └─ ❌ 任一失败 → 整体回滚，MEM 不执行
  │   │
  │   ├── [阶段 2: MEM 路径 ── 后生效（仅 FILE 全部成功后）]
  │   │   ├─ 读取内存字节
  │   │   ├─ Gate Chain 校验
  │   │   ├─ MemAuditLog.Record() → 写入审计日志
  │   │   ├─ sync_path: ConfigManager.load_config() → ACTIVE
  │   │   └─ ❌ 失败 → Report 标记 FAILED（磁盘无副作用）
  │   │
  │   └─ 返回 Report*
  │
  └─ Report 检查: 状态 = SUCCESS → 配置已生效
```

## 关键设计决策

1. **MEM 路径不持久化**：内存配置（`AddMemPath`）只进入 Kernel，不写入持久化存储，重启后需重新提交。审计日志由 `MemAuditLog` 独立记录。
2. **先 FILE 再 MEM**：确保持久化基线先落盘，再应用内存热补丁。FILE 失败时整体回滚，不会出现"磁盘旧 + 内存新"的混合状态。
3. **Session 层翻译查询 key**：`GetConfig(key)` 自动为 MEM key 补 `mem://` 前缀，FILE key 保持原 URI，对 App 透明。
4. **kHttp/kHttps 预留**：枚举值已保留但未实现网络读取能力，传参返回 `NOT_IMPLEMENTED`。

## 与业界架构对比

| 维度 | BlessStar | Kubelet | Spring Cloud | ZooKeeper |
|------|-----------|---------|-------------|-----------|
| 输入模型 | IR 指令 | Pod Spec | PropertySource | ZNode |
| 配置生命周期 | 状态机驱动 | 声明式 reconcile | Environment 刷新 | Watcher 事件 |
| 门禁 | 三阶段链 | Admission Webhook | 无内置 | ACL |
| 持久化 | WAL + CAS + Manifest | etcd | 无内置（文件/DB） | ZAB 协议 |
| 执行引擎 | Pipeline + Pool | Sync/Async workers | 无 | 无 |
| 通知模型 | Watch + EventBus | Informer | RefreshScope | Watcher |
