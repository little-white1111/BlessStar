---
name: "architect-pro"
description: "从业务需求出发，基于骨-肉-血三阶段设计完整的系统架构（业务能力建模 → 技术实现支撑 → 配置治理注入），按需识别配置扩展点并自动生成 config-schema.yaml 和门禁规则。适用于从 0 搭建业务系统，或对已有系统进行 BlessStar 接入改造。"
---

# 系统架构设计师 (Architect Pro) — 骨-肉-血三阶段版

## 核心理念

**"架构设计是骨，技术实现是肉，配置治理是血"**

好的系统建设遵循"骨 → 肉 → 血"的自然递进：

| 阶段 | 对应 | 关注什么 | BlessStar 依赖 | 可跳过？ |
|:-----|:-----|:---------|:---------------|:---------|
| **🦴 骨 (Skeleton)** | 业务能力建模 | 系统要做什么 — 限界上下文、聚合根、用例、不变量 | 零依赖 | ❌ 不可跳过 |
| **🥩 肉 (Flesh)** | 技术实现支撑 | 系统怎么做 — 语言/框架、算法/数据结构、API/Schema | 弱依赖（预留 Port 位置） | ❌ 不可跳过 |
| **🩸 血 (Blood)** | 配置治理注入 | 怎么让系统活起来 — 提取可变参数、生成 Schema、门禁 | 强依赖（引入 BlessStar 引擎） | ✅ **可独立跳过** |

**核心原则**：
- 骨和肉完成时，业务系统已具备**完整功能**，可独立交付、测试、运行
- 血是增强层（血液循环系统），将硬编码/本地配置升级为动态治理，而非重建系统
- 血阶段对业务源代码的侵入**仅限于启动类**，`internal/domain` 核心业务代码零改动

---

## 本 Skill 作为多 Agent 编排器

```
Phase 0 ─→ Phase 1 ────────→ Phase 2 ──────────→ Phase 3
 (评估)     (铸骨)           (填肉)              (活血)
   │          │                │                   │
   ▼          ▼                ▼                   ▼
  诊断   本 skill 直接    subtask-a(三方案)    configdesigner
  门     输出(业务模型)  + 本 skill(技术设计)   → cdd
```

| 步骤 | 调用的子 Agent | 产出 |
|:-----|:--------------|:-----|
| **Phase 0** | 本 skill 直接执行（代码扫描） | 接入状态评估报告 + 改造方案 |
| **Phase 1 (骨)** | 本 skill 直接输出（不调子 Agent） | 业务域模型：用例、实体、业务不变量、可变参数候选 |
| **Phase 2 (肉)** | `subtask-a`（三方案）+ 本 skill（技术设计） | 三方案对比 ADR + 技术选型 + Port 接口定义 |
| **Phase 3 (血)** | `configdesigner` → `cdd` | `config-schema.yaml` + `gate_rule_def.json` |

---

## 跨 Agent 数据契约（Data Contract v1.0）

```yaml
# Data Contract v1.0 — architect-pro ↔ subtask-a ↔ configdesigner ↔ cdd

# 契约 A：subtask-a → architect-pro（Phase 2 输出）
contract_subtask_a_output:
  chosen_scheme: "A|B|C"          # 必填，用户最终确认的方案
  domain: string                   # 必填，业务域
  core_use_cases: []string         # 必填，至少 3 个核心用例
  invariants: []string             # 必填，至少 5 条业务不变量
  topology_ascii: string           # 必填，ASCII 架构拓扑图（纯业务视角）
  config_fields: []                # 可选，识别出的可变参数
    - key: string                  #   配置字段名
      type: "I32|STR|DURATION|..."
      default: any                 #   生产环境默认值
      business_desc: string        #   业务描述

# 契约 B：configdesigner → architect-pro（Phase 3 输出）
contract_configdesigner_output:
  has_contract: bool               # 是否有 contract 段
  yaml_fields: []                  # 完整的 fields 列表
    - key: string
      has_contract: bool
      contract: any                # range / dependencies / slo_impact（如有）

# 契约 C：cdd → architect-pro（Phase 3 输出）
contract_cdd_output:
  gate_rules: []                   # 完整的 gate_rule_def 数组
  compile_time_rules: []           # 带 _COMPILE_TIME 后缀的规则
```

**数据校验规则**（Phase 3 汇总时执行）：
1. `contract_subtask_a_output.chosen_scheme` 必须为 `A`、`B` 或 `C`，否则终止
2. 如果 `chosen_scheme` 为 `B` 或 `C`，则 `config_fields` 必须存在且长度 ≥ 3
3. 如果 `contract_configdesigner_output.has_contract == true`，则 `yaml_fields` 中至少有一个 `has_contract == true`
4. 如果 `contract_cdd_output.compile_time_rules` 非空，则每条规则的 `scenario` 必须以 `_COMPILE_TIME` 结尾

---

## 核心约束（必须遵守）

### 1. 三方案铁律
每次架构讨论必须输出 **3 个候选方案**：

| 方案 | 侧重 | 配置治理深度 | 适用场景 |
|:-----|:-----|:------------|:---------|
| **方案A（极简运维）** | 最小依赖，最快落地 | 无或极简（环境变量/本地文件） | 内部工具、初创 MVP、配置字段 ≤3 |
| **方案B（均衡架构）** | ⬅️ **默认推荐** | 中等（Schema + 门禁） | 生产业务系统、多团队协作、配置字段 ≥5 |
| **方案C（高扩展）** | 面向未来增长 | 深度（契约驱动 + 事件总线） | 大型分布式、多系统共用 Schema、配置字段 ≥15 |

### 2. ASCII 架构拓扑图强制
用户确认的最终草案中，**必须**包含 ASCII 字符绘制的系统拓扑图。
- Phase 1（骨）：业务视角拓扑图（服务、消息、存储等组件及箭头流向）
- Phase 2（肉）：技术视角拓扑图（在业务拓扑上标注协议、中间件、数据流）
- Phase 3（血）：在拓扑图上标注 `config-schema.yaml` 和 `SchemaLoader` 位置

### 3. 架构不变量显式化
在 Phase 1 明确列出**业务不变量**（如：订单状态不可回退、支付金额不可篡改），Phase 2 追加**技术不变量**（如：禁止循环依赖、全链路 TraceID）。

### 4. 零业务侵入承诺（关键！）
在 Phase 3（血）接入 BlessStar 时，所有改动限制在 `adapter/` 和 `provider/` 目录：
- **不改** `internal/domain/` 核心业务代码
- **不改** `internal/service/` 用例编排逻辑
- **只新增** `internal/port/` 接口定义 + `internal/adapter/blessstar/` 实现
- **只修改** 启动类（`main.go` / `Program.cs` 等）完成依赖注入

### 5. 血阶段可插拔原则
**血阶段可以独立跳过**。即使用户在 Phase 3 选择不接入 BlessStar，Phase 1 + Phase 2 产出的架构方案仍然是完整的、可独立交付的业务系统。ADR 中必须明确标注："当前方案未启用 BlessStar 配置治理，后续可通过 `blessstar-codegen --schema config-schema.yaml --lang go` 零侵入接入"。

### 6. 任务列表强制追踪（执行红线）
本 skill 的每个 Phase 和 Step 执行前，**必须**使用 `TodoWrite` 工具创建对应的任务列表并标记进度：

**Phase 级任务列表**（确认 phase 路线后立即创建）：
```markdown
待办项示例（进入 Phase 1 时创建）：
  "Phase 1: 🦴 铸骨 — 业务能力建模"
    ├─ "Step 1: 需求澄清与核心设计目标"
    ├─ "Step 1.5: 业务建模与骨架输出"
    └─ "产出 8 项硬性交付物并移交 Phase 2"
```

**Step 级任务列表**（进入每个 step 时细化）：
```markdown
待办项示例（进入 Step 1.5 时创建）：
  "Step 1.5: 业务建模与骨架输出"
    ├─ "A. 绘制业务流程图（ASCII）"
    ├─ "B. 绘制核心类图（ASCII）"
    ├─ "C. 设计骨架文件目录树"
    └─ "检查 8 项交付物完整性 → 进入 Phase 2"
```

**强制规则**：
- **Phase 路线确认后**：立即创建 Phase 级 TodoWrite，标记当前执行的 Phase 为 `in_progress`，其余为 `pending`
- **进入每个 Step 前**：细化该 Step 的子任务列表，标记第一条为 `in_progress`
- **Step 完成后**：立即标记为 `completed`，并开始下一个 Step
- **Phase 完成后**：标记该 Phase 所有任务为 `completed`，将下一个 Phase 第一条任务设为 `in_progress`
- **禁止跳步**：不允许在未创建任务列表的情况下直接执行任何 Step 的内容输出
- **进度可见性**：所有 TodoWrite 操作必须附带 `summary` 字段，描述本步实际完成的工作

---

## 场景模式

本 skill 支持两种入口场景，AI 应在 Phase 0 中自动识别：

### 模式 A — 全新系统设计（默认）
从零推导架构。直接进入 Phase 1（铸骨）。

### 模式 B — 现有系统改造（升级迁移）
当用户说 "改造 / 迁移 / 升级 / 接入 BlessStar" 等时，自动进入 **Phase 0 评估门**。

---

# 标准工作流

```
┌─────────────────────────────────────────────────────────────────┐
│ Phase 0: 🩺 BlessStar 接入状态评估（仅模式 B）                    │
│   ├─ 代码扫描 → 状态判定 → 改造策略生成                            │
│   └─ 通过后进入 Phase 1                                          │
├─────────────────────────────────────────────────────────────────┤
│ Phase 1: 🦴 铸骨 — 业务能力建模                                  │
│   ├─ Step 1: 需求澄清（业务域 + 功能 + 非功能需求）                │
│   │   + 识别可变参数候选（仅标记，不决策）                         │
│   ├─ Step 1.5: 业务建模与骨架输出                                 │
│   │   ├─ 业务流程图（ASCII）                                      │
│   │   ├─ 核心类图（ASCII）                                        │
│   │   └─ 骨架文件目录树                                           │
│   └─ 产出: 8 项硬性交付物 → Phase 2 强制输入                       │
├─────────────────────────────────────────────────────────────────┤
│ Phase 2: 🥩 填肉 — 技术实现支撑                                  │
│   ├─ Step 2: 三方案对比与架构设计（subtask-a）                    │
│   ├─ Step 2.5: 技术选型与核心设计 + Port 接口定义                 │
│   └─ 产出: 技术架构 ADR（三方案 + 拓扑图 + Schema + 模块拆分）      │
├─────────────────────────────────────────────────────────────────┤
│ Phase 3: 🩸 活血 — 配置治理注入（可选）                           │
│   ├─ Step 3: configdesigner（提取配置 → 生成 YAML）              │
│   ├─ Step 4: cdd（生成门禁规则）                                 │
│   └─ Step 5: 汇总整合 → 完整 ADR                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Phase 0 — BlessStar 接入状态评估（仅模式 B）

当检测到用户意图为"改造现有系统"时，**在 Phase 1 之前**强制执行以下三问诊断：

### A. 自动代码扫描
AI 主动询问或扫描用户提供的代码结构：

| 检查项 | 检查内容 | 判定依据 |
|:-------|:---------|:---------|
| SDK 依赖 | `go.mod` / `package.json` / `pom.xml` 中是否有 BlessStar SDK 引用 | 有/无 |
| 初始化代码 | `main.go` / `Program.cs` 中是否有 BlessStar 客户端初始化 | 有/无 |
| Port 接口层 | 是否存在 `internal/port/` 或 `pkg/ports/` 配置读取接口 | 有/无 |
| 当前配置方式 | 使用环境变量、本地配置文件、硬编码，还是已有配置中心 | 类型判定 |

### B. 状态判定与分级策略

| 状态 | 判定条件 | 改造策略 | 侵入程度 |
|:-----|:---------|:---------|:---------|
| **🔴 未接入 (0%)** | 无 SDK、无 Port、代码直接 `os.Getenv` 或硬编码常量 | **绞杀者模式 (Strangler Fig)**：新建 `internal/port/` + `internal/adapter/blessstar/`，将旧配置包标记 `@Deprecated` | 仅新增文件，不修改业务代码 |
| **🟡 半接入 (50%)** | 已引入 SDK，但业务代码直接调用 `client.Get()`，缺乏 Port 抽象层 | **抽取接口 (Extract Interface)**：为 BlessStar 调用类提取 Port 接口，业务代码改为依赖接口 | IDE 自动重构，无逻辑变更 |
| **🟢 完全接入 (100%)** | 已有 `internal/port/` + `adapter_blessstar` 分离结构 | **无需改造**，直接进入 Phase 3 的配置治理优化 | 零改动 |

### C. 生成迁移手术方案

- **防腐层模式（ACL）**：不修改 `internal/domain` 核心逻辑，只在 `adapter_blessstar` 包中实现"存量配置迁移适配器"
- **双写/代理模式**：将现有的 `viper` / `os.Getenv` / 本地 JSON 封装成一个 `LegacyConfigReader`，它实现 `ports.ConfigReader` 接口
- **灰度替换**：业务代码从"直接读配置"变为"依赖 Port 接口"，底层既可以读 BlessStar，也可以读回旧的配置文件

```
改造前：
  internal/service/payment.go
    → os.Getenv("PAYMENT_TIMEOUT")        ← 业务代码直接依赖环境变量

改造后：
  internal/service/payment.go
    → this.config.Timeout()               ← 依赖 Port 接口

  internal/adapter/legacy/
    → LegacyConfigReader implements Port  ← 从旧配置源读取（零业务改动）

  internal/adapter/blessstar/
    → BlessStarConfigReader implements Port  ← 从 BlessStar 读取（新接入）
```

### D. 零业务侵入承诺宣誓
明确告知用户：
> "以下所有改动限制在 `adapter/` 和 `provider/` 目录，`internal/domain` 核心业务代码**完全不受影响**。系统原有的配置文件（如 `application.yml`、`config.json`）仍然可读，接入 BlessStar 后只是多了一个动态配置源，而非替换。"

**决策门 ⛩️**：
- 如果 `完全接入 (100%)` → **直接进入 Phase 3**
- 如果 `未接入 (0%)` 或 `半接入 (50%)` → **进入 Phase 1**，先完成业务架构设计
- 如果用户放弃接入 BlessStar → **退出本 skill**

---

## Phase 1 — 🦴 铸骨：业务能力建模

**核心关注点**：业务要做什么，不关注具体用什么技术实现。零 BlessStar 依赖。

**此阶段不调用子 Agent**，由本 skill 直接与用户交互完成。

### Step 1 — 需求澄清与核心设计目标

**A. 确认业务域与功能边界**
- 业务域名称（如 `payment`、`order`、`auth`）
- 核心功能列表（至少 3 个主要用例）
- 系统边界（与外部系统的交互方式）

**B. 确认非功能性需求**
- 性能要求（预估 QPS、延迟要求）
- 可用性要求（SLA 目标、灾备策略）
- 扩展性要求（未来 6-12 个月的增长预期）
- 安全性要求（认证授权、数据加密）

**C. 识别可变参数候选（配置种子）**
引导用户识别当前/未来可能变化的业务参数，**但不强制数量**：

> "以下哪些参数是当前硬编码但将来可能需要动态调整的？此阶段仅识别，不决定是否使用配置中心。"
> - 超时时间、重试次数、限流阈值
> - 费率、折扣系数、计费规则
> - 功能开关、灰度策略
> - 其他业务决策点

**D. 引导原则**
- 如果识别出 ≥ 3 个可变参数：提示 "当前识别出 {n} 个可变参数，后续可在 Phase 3（活血）中评估是否需要引入 BlessStar 动态治理"
- 如果 < 3 个：提示 "可变参数较少，Phase 3 中可能不需要专门的配置治理，通过环境变量即可满足"

**关于技术实现的约定**：
> 此阶段**严禁讨论**具体技术栈（Go/Java/TypeScript）、框架（Gin/Spring Boot）、数据库（MySQL/PostgreSQL）。这些属于 Phase 2（填肉）的范畴。Phase 1 只讨论业务逻辑和系统行为。但**业务流程图和核心类图**属于"业务建模"范畴（用例的图形化表达），可在本阶段使用 UML / 流程图符号绘制，不涉及具体技术实现。

---

### Step 1.5 — 业务建模与骨架输出

在 Step 1 的需求确认基础上，产出三份**硬性交付物**，作为 Phase 2（填肉）的强制输入：

#### A. 业务流程图（ASCII 必填）
用 ASCII 字符绘制端到端业务流程，覆盖 Step 1 确认的 ≥ 3 个核心用例：

```
┌──────────┐    ┌──────────┐    ┌──────────┐
│ 用户创建  │    │ 支付回调  │    │ 退款流程  │
│  订单    │    │  处理    │    │          │
└────┬─────┘    └────┬─────┘    └────┬─────┘
     │               │               │
     ▼               ▼               ▼
┌─────────────────────────────────────────┐
│           支付服务 (Payment Service)       │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  │
│  │ 订单管理  │  │ 支付网关  │  │ 退款处理  │  │
│  └─────────┘  └─────────┘  └─────────┘  │
└─────────────────────────────────────────┘
     │               │               │
     ▼               ▼               ▼
┌──────────┐    ┌──────────┐    ┌──────────┐
│  订单DB   │    │ 支付网关  │    │  退款DB   │
│          │    │ (外部)   │    │          │
└──────────┘    └──────────┘    └──────────┘
```

#### B. 核心类图（ASCII 必填）
用 UML 风格 ASCII 绘制核心实体及其关系，**只含业务属性，不含技术注解**：

```
┌─────────────────┐         ┌─────────────────┐
│     Order       │         │    Payment       │
├─────────────────┤         ├─────────────────┤
│ - orderId       │◄────────│ - paymentId     │
│ - userId        │  1:N    │ - orderId       │
│ - amount        │         │ - amount        │
│ - status        │         │ - method        │
│ - createdAt     │         │ - status        │
├─────────────────┤         ├─────────────────┤
│ + create()      │         │ + process()     │
│ + cancel()      │         │ + refund()      │
│ + getStatus()   │         │ + getStatus()   │
└─────────────────┘         └─────────────────┘
        │
        │ 1:N
        ▼
┌─────────────────┐
│   Refund        │
├─────────────────┤
│ - refundId      │
│ - paymentId     │
│ - amount        │
│ - reason        │
│ - status        │
├─────────────────┤
│ + apply()       │
│ + approve()     │
│ + reject()      │
└─────────────────┘
```

#### C. 骨架文件目录树（必填）
基于业务实体和聚合根推导出目录树，**不含具体代码实现，只含文件占位**：

```
internal/
├── domain/              # 业务实体与聚合根
│   ├── order.go         (实体: Order)
│   ├── payment.go       (实体: Payment)
│   └── refund.go        (实体: Refund)
├── service/             # 用例编排
│   ├── order_service.go       (用例: 创建订单)
│   ├── payment_service.go     (用例: 支付回调处理)
│   └── refund_service.go      (用例: 退款)
├── port/                # 端口接口（Phase 2 填入）
│   └── .gitkeep
└── cmd/
    └── main.go          (启动入口占位)
```

目录树原则：
- `domain/` 中的文件：占位即可，标注对应的实体名
- `service/` 中的文件：每个文件标注对应的用例名
- `port/`、`adapter/`、`provider/`：用 `.gitkeep` 或空目录占位，具体内容由 Phase 2/3 填充
- **严禁**在文件占位中写入任何实现代码（如 import 语句、函数体）

---

**Phase 1 产出物**（均为 Phase 2 的硬性输入）：

| # | 产出物 | 格式 | 说明 |
|:--|:-------|:-----|:-----|
| 1 | 业务域名称与边界定义 | 文本 | 含系统边界与外部交互方式 |
| 2 | 核心用例列表 | 列表 | ≥ 3 个，含简要描述 |
| 3 | 业务不变量 | 编号列表 | ≥ 5 条，如：订单金额不可篡改、状态变迁单向不可逆 |
| 4 | 可变参数候选 `config_fields[]` | YAML 列表 | 可能为空 |
| 5 | 业务实体与聚合根定义 | 文本 | 含核心属性与行为 |
| 6 | **业务流程图** | ASCII | 端到端流程，覆盖全部核心用例 |
| 7 | **核心类图** | ASCII | UML 风格，只含业务属性 |
| 8 | **骨架文件目录树** | 目录树 | 含文件占位，标注实体/用例对应关系 |

**决策门 ⛩️**：
- 始终进入 **Phase 2（填肉）** — 骨已铸成，上述 8 项产出物将作为 subtask-a 和后续技术设计的强制输入

---

## Phase 2 — 🥩 填肉：技术实现支撑

**核心关注点**：在骨架上填充技术实现细节。弱 BlessStar 依赖（只预留 Port 位置，不落盘 YAML）。

### Step 2 — 三方案对比与架构设计

→ 调用子 Agent：**Use Skill: subtask-a**

**输入**：Phase 1 产出的全部 8 项交付物（详见 Phase 1 产出物表），其中骨架文件目录树、业务流程图、核心类图是 subtask-a 输出三方案对比和架构拓扑图的**硬性前置依赖**

**输出预期**：`架构方案选择记录.md`
- 三方案对比表格（方案 A/B/C），含**配置治理深度**列
- ASCII 架构拓扑图（服务、消息、存储组件及箭头流向）
- 技术层面的不变量追加（如：禁止循环依赖、全链路 TraceID）
- 从 `config_fields[]` 评估哪些字段需要进入 Phase 3 的 Schema 管理

**等待 subtask-a 完成后**，提取结构化摘要：

```yaml
sub_agent_summary:
  chosen_scheme: "A|B|C"              # 用户最终确认的方案
  domain: "payment"
  config_fields:                      # 确认后的配置字段（可能为空）
    - key: "payment.timeout"
      type: "DURATION"
      default: "30s"
      business_desc: "支付超时时间"
  invariants: []string                # 至少 5 条（业务 + 技术）
  topology_ascii: string              # ASCII 架构拓扑图
```

**决策门 ⛩️**：
- 始终进入 **Step 2.5（技术选型与核心设计）** — 方案已选定，需要确定具体技术细节

---

### Step 2.5 — 技术选型与核心设计

基于 Phase 1 的业务架构 + subtask-a 选定的方案，明确以下技术决策：

**A. 技术栈选型**
- 语言：Go / Java / TypeScript / Rust / C++
- Web 框架：Gin / Spring Boot / Express / Axum
- 数据库：MySQL / PostgreSQL / SQLite / MongoDB
- 缓存：Redis / BigCache / 本地内存
- 消息队列：Kafka / RabbitMQ / NATS
- 部署方式：裸机 / Docker / K8s

**B. 数据 Schema 设计**
- 核心表结构（ER 图或字段列表）
- 索引策略
- 数据生命周期（归档、清理）

**C. API 协议设计**
- 对外 API：REST / gRPC / GraphQL
- 内部通信：同步 RPC / 事件驱动 / 消息队列
- 关键接口路径及请求/响应格式

**D. Port 接口定义（预留 BlessStar 位置）**

Port 接口是"肉"连接"血"的关键桥梁。在此阶段**只定义方法签名，不提供实现**：

```typescript
// internal/port/config-reader.ts
// Port 接口 — 纯抽象，零实现
// 具体实现在 Phase 3（活血）中由 blessstar-codegen 生成
export interface ConfigReader {
  get(path: string): Promise<unknown>;
}

// internal/port/payment-config.ts
// 业务域配置 Port — 只定义方法签名
export interface PaymentConfig {
  timeout(): Promise<number>;
  maxRetry(): Promise<number>;
  feeRate(): Promise<number>;
}
```

Port 接口设计原则（参照 [livedesign 源码](file:///c:/Users/LJHlj/blessstar/BlessStar/business system/livedesign/src/ports/config-reader.ts) 的 Port-Adapter 模式）：
- **接口要薄**：每个方法只返回一个配置值，不做聚合
- **返回 `Promise<unknown>`**：允许 Adapter 层进行类型断言
- **不依赖任何外部库**：Port 层是纯抽象，无第三方依赖
- **三阶段降级承诺**：所有 Adapter 实现需遵循"ConfigReader 实时查询 → LastKnownGood 缓存 → 硬编码默认值"的降级策略

**E. 模块拆分与目录结构**

```
internal/
├── domain/          # 业务实体与聚合根（Phase 1 产物）
├── service/         # 用例编排（Phase 1 产物）
├── port/            # 端口接口定义（Phase 2 产物，血阶段的桥梁）
│   ├── config-reader.ts
│   └── payment-config.ts
├── adapter/         # 端口实现（Phase 3 填入，或手动实现）
│   ├── http/        # HTTP 客户端适配器
│   ├── rpc/         # RPC 客户端适配器
│   └── legacy/      # 旧配置源适配器（Phase 0 改造用）
├── provider/        # 依赖注入（Phase 3 填入）
└── cmd/             # 启动入口
```

**产出物**：
- 技术选型决策记录（嵌入 ADR）
- 数据 Schema 设计
- API 协议定义
- Port 接口定义文件（仅签名，无实现）
- 模块拆分与目录树

**决策门 ⛩️**：
- 如果 Phase 1 识别的 `config_fields` 长度 ≥ 3，且用户确认需要动态配置治理 → **进入 Phase 3（活血）**
- 如果 `config_fields` < 3，或用户选择不使用动态配置 → **直接汇总输出完整 ADR（不含血阶段）**

---

## Phase 3 — 🩸 活血：配置治理注入（可选阶段）

**核心关注点**：为系统注入"动态调节"能力。强 BlessStar 依赖，引入配置引擎和门禁规则。

**进入条件**（必须同时满足）：
1. Phase 1 识别的 `config_fields` 长度 ≥ 3
2. 用户确认需要运行时动态调整配置
3. 用户确认接受 BlessStar 作为配置治理引擎

### Step 3 — 调用 configdesigner 提取配置元数据

→ 调用子 Agent：**Use Skill: configdesigner**

**输入**：
- Phase 1 提取的 `config_fields[]` 列表（key + type + default + business_desc）
- Phase 2 确定的 Port 接口定义（作为生成 Adapter 的输入）
- `--biz-id <biz_id>`（如用户已提供）
- `--format yaml`（默认）

**输出预期**：`config-schema.yaml`

```yaml
# 输出示例
domain: payment
version: v1.0.0
fields:
  - key: payment.timeout
    type: DURATION
    default: "30s"
    contract:
      range: [10, 120]
      dependencies:
        - "payment.timeout > payment.max_retry * 2"
      slo_impact: "支付成功率"
    ui_meta:
      label: "超时时间"
      order: 1
```

**等待 configdesigner 完成后**，提取结构化摘要：

```yaml
configdesigner_summary:
  has_contract: true
  yaml_fields:
    - key: "payment.timeout"
      has_contract: true
    - key: "payment.max_retry"
      has_contract: true
```

**决策门 ⛩️（Step 3 → 人工确认 → Step 4）**：
- 如果 configdesigner 调用成功且 `has_contract == true` → **进入人工确认门**
- 如果 configdesigner 调用成功但 `has_contract == false` → **跳跃至 Step 5**（跳过 cdd）
- 如果 configdesigner 调用失败 → **进入 Step 3 降级路径**：
  - 在 ADR 中标记 `⚠️ 配置契约生成失败，需人工补录 config-schema.yaml`
  - 仍继续输出不含门禁的架构方案

**人工确认门**：
configdesigner 产出 `config-schema.yaml` 后，**必须向用户展示完整 YAML 内容**：

> "请确认 `config-schema.yaml` 中的 `contract` 段（range/dependencies/slo_impact）是否符合业务预期。如需调整，请说明修改内容；如确认无误，输入 '继续' 进入门禁规则生成。"

- 如果用户提出修改 → **人工更新 YAML** 后，将修改后的 YAML 作为 Step 4 的输入
- 如果用户确认 → **进入 Step 4**

---

### Step 4 — 调用 cdd 生成门禁规则

→ 调用子 Agent：**Use Skill: cdd**

**输入**：Step 3 产出的完整 `config-schema.yaml`

**输出预期**：`gate_rule_def.json`

```json
[
  {
    "field_key": "payment.timeout",
    "field_type": "INT32",
    "gate_type": "RANGE",
    "params": { "min": 10, "max": 120 },
    "scenario": "payment_timeout_range_config",
    "sub_category": "threshold",
    "stable_key": "payment:payment:payment.timeout:0:threshold:range",
    "error_hint": "支付超时时间必须在 10~120 秒之间"
  }
]
```

**决策门 ⛩️（Step 4 → Step 5）**：
- 如果 cdd 调用成功 → **进入 Step 5**
- 如果 cdd 调用失败 → **进入 Step 4 降级路径**：
  - 在 ADR 中标记 `⚠️ 门禁规则生成失败，请手动运行 cdd 补录`
  - config-schema.yaml 已就绪，业务系统可先按无门禁模式运行
  - 不影响 Step 5 汇总

---

### Step 5 — 汇总整合（回到本 skill）

收集所有子 Agent 的产出物，聚合为最终整合版 `架构方案选择记录.md`。

| 输入源 | 产物 | 整合位置 |
|:-------|:-----|:---------|
| subtask-a | 原始 ADR（三方案 + 业务不变量 + 拓扑图） | 直接继承，作为骨架 |
| Phase 2 | 技术选型 + Port 接口定义 + 模块拆分 | 嵌入 ADR 的"技术设计"章节 |
| configdesigner | `config-schema.yaml` | 嵌入 ADR 的"配置与契约"章节 |
| cdd | `gate_rule_def.json` | 嵌入 ADR 的"门禁规则"章节 |

#### 一致性检查（汇总前必须执行）

对比 Phase 1 识别的 `config_fields[]` 与 Step 3 中 configdesigner 产出的 `yaml_fields[]`：

- 如果发现差异（如 Phase 1 有 `payment.timeout` 但 Step 3 没有）：
  - 在 ADR 中标记 `⚠️ 检测到配置字段列表不一致：subtask-a 识别了 {n} 个，configdesigner 生成了 {m} 个`
  - **提示用户确认**，是否需要在 config-schema.yaml 中补全缺失字段
  - 如果用户确认补全 → **返回 Step 3**，重新调用 configdesigner
  - 如果用户确认可忽略 → 继续 Step 5
- 如果一致 → 直接进入 Step 5 汇总

#### 数据契约校验
- 校验不通过 → 在 ADR 中标记 `⚠️ 数据契约校验失败：{具体规则}` 并终止

#### 整合后的 ADR 必须包含以下章节

##### 1. 📐 业务架构拓扑图（Phase 1 产出）
subtask-a 的 ASCII 业务拓扑图。

##### 2. 🥩 技术架构拓扑图（Phase 2 产出）
在业务拓扑图上标注协议（REST/gRPC）、中间件（Redis/Kafka）、数据库。

##### 3. 🩸 配置拓扑图（Phase 3 产出——如有）
在拓扑图上标注 `config-schema.yaml` 和 `SchemaLoader` 的位置。

##### 4. ⚙️ 架构不变量（最终版）
- **业务不变量**（Phase 1 继承）：至少 5 条
- **技术不变量**（Phase 2 追加）：如"禁止循环依赖""全链路 TraceID"
- **配置不变量**（Phase 3 追加——如有）：如"门控不可绕过""Schema 版本兼容"

##### 5. 📦 模块拆分与目录结构（Phase 2 产出）

```
internal/
├── domain/              # 业务实体与聚合根
│   └── payment.go
├── service/             # 用例编排
│   └── payment_service.go
├── port/                # 端口接口定义（血阶段的桥梁）
│   ├── config-reader.ts
│   └── payment-config.ts
├── adapter/             # 端口实现
│   ├── blessstar/       # codegen 生成（Phase 3）
│   ├── mock/            # codegen 生成（测试用）
│   ├── legacy/          # 旧配置源兼容（Phase 0 改造用）
│   └── http/            # 外部服务调用
└── provider/            # 依赖注入（Phase 3 填入）
    └── index.ts
```

##### 6. 🎛️ 配置与契约（Phase 3 产出——如有）

- 完整 `config-schema.yaml` 内容
- 门禁规则摘要（`gate_rule_def.json` 中的规则列表）
- `config-schema.yaml` 存放路径：仓库根目录
- 导入 BlessStar 的方式：
  ```bash
  # 方式1：schema_watcher 热挂载（推荐）
  cp config-schema.yaml /etc/blessstar/schemas/<domain>.yaml

  # 方式2：codegen 编译时注册
  blessstar-codegen --schema config-schema.yaml --lang go
  ```

##### 7. 🧪 测试策略

| 测试层级 | 覆盖范围 | 工具/框架 |
|:---------|:---------|:----------|
| **单元测试** | Service 层 + Domain 层 | Go testing / Vitest |
| **Port Mock 测试** | 使用 `adapters/mock/` 替换配置源 | 自动生成 |
| **集成测试** | 数据库 + 外部服务 | Docker Compose |
| **契约测试**（如有配置） | 边界值测试用例 | 自动生成 |
| **回归测试** | 配置变更后需运行的测试集 | CI 流水线 |

##### 8. 🚀 落地步骤

**Phase 1 + Phase 2（骨+肉）— 无论是否接入 BlessStar 都必须执行：**
1. 创建仓库与基础骨架
2. 按目录结构创建模块
3. 实现 domain 实体与聚合根
4. 实现 service 用例编排
5. 实现 port 接口定义
6. 实现 adapter/http、adapter/rpc 等外部服务适配器
7. 编写单元测试
8. 配置 CI 流水线

**Phase 3（血）— 可选，仅在需要配置治理时执行：**
9. 将 `config-schema.yaml` 放置在仓库根目录
10. 运行 `blessstar-codegen --schema config-schema.yaml --lang go --output internal/adapter/blessstar`
11. 在启动类中注入 BlessStar Adapter
12. 部署 SchemaLoader 监听目录
13. 配置门禁规则

**业务系统零侵入验证清单：**
- [ ] `internal/domain/` 未修改
- [ ] `internal/service/` 未修改
- [ ] 所有新增文件在 `internal/port/`、`internal/adapter/`、`internal/provider/` 下
- [ ] 仅启动类（`cmd/main.go`）修改了依赖注入

#### 产出物规范

| 产出物 | 格式 | 存放路径 | 必须 |
|:-------|:-----|:---------|:-----|
| 架构方案选择记录 | `.md` | `docs/architecture/ADR-<date>-<domain>.md` | ✅ 必须 |
| 业务架构拓扑图 | ASCII 文本 | 内嵌于 ADR | ✅ 必须 |
| 技术架构拓扑图 | ASCII 文本 | 内嵌于 ADR | ✅ 必须 |
| 架构不变量 | 编号列表 | 内嵌于 ADR | ✅ 必须 |
| Port 接口定义 | `.ts`/`.go` | `internal/port/` | ✅ 必须 |
| 落地建议 | 结构化章节 | 内嵌于 ADR | ✅ 必须 |
| `config-schema.yaml` | `.yaml` | 仓库根目录 | 仅 Phase 3 |
| 门禁规则 JSON | `.json` | codegen 输出 | 仅 Phase 3 |
| Adapter 实现 | `.ts`/`.go` | `internal/adapter/blessstar/` | 仅 Phase 3 |

---

## 退出条件

在以下情况**必须退出本 skill**：
1. 用户要求编写具体业务代码（本 skill 只产出 `.md`、`.yaml`、Port 接口签名 `.ts`/`.go`，不产出完整业务实现）
2. 用户要求绕过架构设计直接进入编码（应先完成 ADR）
3. 用户在 Phase 1 中明确表示该模块无架构设计必要（如单函数脚本）

---

## 与相邻 skill 的协作边界

| Skill | 输入 | 输出 | 依赖关系 |
|:------|:-----|:-----|:---------|
| `architect-pro`（本 skill / 编排器） | 业务域 + 功能需求 + 非功能需求 | ADR.md + Port 接口 + 可选 config-schema.yaml + 门禁 JSON | **编排 subtask-a → configdesigner → cdd** |
| `subtask-a`（架构设计） | Phase 1 产出的业务域模型 + 非功能性需求 | 架构方案选择记录（三方案） | 被本 skill Phase 2 调用 |
| `configdesigner`（配置提取） | 源码 + 配置字段列表 | `config-schema.yaml` | 被本 skill Phase 3 调用 |
| `cdd`（门禁规则） | `config-schema.yaml` | 门禁规则 JSON | 被本 skill Phase 3 调用 |
| `subtask-b`（工程落地） | ADR.md 中的落地建议 | 具体代码（`.go`/`.c`/测试） | 依赖本 skill 的最终 ADR |

---

## 使用示例

#### 场景 1：全新系统设计
> **用户输入**："我要设计一个支付系统的架构，包括创建订单、支付回调、退款三个核心功能。需要支持高并发，预估 QPS 5000。"

**AI 响应**：
1. **Phase 1（铸骨）**：识别业务域 `payment`，核心用例 3 个，输出业务域模型
2. **Phase 2（填肉）**：调用 subtask-a 输出三方案对比 → 选定方案 B → 技术选型 Go + Gin + PostgreSQL + Redis → 定义 `PaymentConfig` Port 接口
3. 未识别出 ≥ 3 个可变参数 → 输出不含血阶段的完整 ADR

#### 场景 2：已有系统接入 BlessStar
> **用户输入**："我们有个支付服务已经上线了，配置是写在 application.yml 里的，想接入 BlessStar 做动态配置。"

**AI 响应**：
1. **Phase 0（评估）**：扫描发现无 SDK、无 Port、直接 `@Value` 注解 → **🔴 未接入 (0%)**
2. 建议绞杀者模式：新建 `internal/port/PaymentConfig.java` + `adapter/legacy/LegacyConfigReader.java`
3. **Phase 1（铸骨）**：确认业务边界，识别 5 个可变参数
4. **Phase 2（填肉）**：定义 Port 接口，预留 `adapter/blessstar/` 位置
5. **Phase 3（活血）**：生成 `config-schema.yaml` → 用户确认 → 生成门禁规则
6. 最终 ADR 包含：业务拓扑 + 技术选型 + `config-schema.yaml` + gate_rule_def.json
7. **零侵入承诺**：仅新增 `port/`、`adapter/blessstar/`、`provider/`，原有 `application.yml` 仍可读

#### 场景 3：已有 Port-Adapter 完整接入（100%）
> **用户输入**："我们的 livedesign 系统已经通过 Port-Adapter 接入了 BlessStar，想优化配置治理。"

**AI 响应**：
1. **Phase 0（评估）**：扫描发现 `ports/config-reader.ts`、`adapters/blessstar/`、`provider/` → **🟢 完全接入 (100%)**
2. 直接进入 **Phase 3**，跳过 Phase 1 + Phase 2
3. 运行 configdesigner 提取现有 Schema → 补充 contract 段 → 生成更完善的门禁规则
4. 输出优化后的 `config-schema.yaml` + gate_rule_def.json

---

## 版本与维护

- **Version**: 2.0.0
- **Last Updated**: 2026-07-13
- **Maintainer**: BlessStar Architecture Team
- **Changelog**:
  - v2.0.0: 重构为骨-肉-血三阶段工作流。新增 Phase 0（BlessStar 接入状态评估），新增零业务侵入承诺机制。血阶段可独立跳过。Port-Adapter 模式作为血阶段的桥梁显式化。
  - v1.0.0: 初始版本，整合 subtask-a + configdesigner + cdd 为多 Agent 编排架构设计 Skill
