---
name: "schema-first-architect"
description: "基于 Schema-First 范式从0搭建业务系统架构。以 config-schema.yaml（配置宪法）为唯一真理源，将契约约束作为架构不变量，推导出完整的模块拆分、端口-适配器、门禁规则。使用场景：全新业务系统架构设计，或 legacy 系统向 Schema-First 迁移的架构规划。入口条件：用户已明确业务域（domain）和核心配置字段列表。"
---

# Schema-First 架构设计师 (Architect)

## 核心理念

**"配置即宪法，其余皆衍生"**

在 Schema-First 范式下，`config-schema.yaml` 不再仅仅是配置文件——它是业务系统的**架构宪法**。所有架构决策（模块拆分、接口定义、门禁规则、测试策略、可观测性）均由此推导，无需人工臆断。

```
config-schema.yaml (宪法)
  │
  ├─→ 模块拆分与端口接口      (Ports & Adapters)
  ├─→ 运行时门禁与 UI 规则    (BlessStar Gate / UI)
  ├─→ 测试双打与边界用例      (Test Doubles / Test Cases)
  └─→ 可观测性与告警策略      (Observability / Alerting)
```

---

## 核心约束（必须遵守）

### 1. 三方案铁律
每次架构讨论必须输出 **3 个候选方案**，分别侧重：
- **方案A（极简运维）**：最小依赖，最快落地。适合配置字段 ≤5 且变更不频繁的场景。
- **方案B（契约驱动）**⬅️ **默认推荐**：以配置约束为第一推动力，架构推导最自然。适合跨域依赖复杂、多团队协作的场景。
- **方案C（高扩展/事件驱动）**：面向未来增长，但引入中间件成本。适合配置字段 ≥20 且多系统共用同一 Schema 的场景。

输出格式：三方案对比表格，包含核心思路、优缺点、预估值、维护成本。

### 2. ASCII 架构拓扑图强制
用户确认的最终草案中，**必须**包含：

```
config-schema.yaml (SSOT)
  ├─→ codegen ──→ ports/*.go, adapters/*.go, gate_configs.h
  ├─→ BlessStar ──→ SchemaLoader → GateFactory → 门禁引擎
  └─→ CI/CD ──→ codegen --check 一致性校验
```

### 3. 不变量显式化
必须在 "架构不变量" 章节列出每条**不可妥协的规则**。**必须**包含以下 5 条源自 "配置即宪法" 的核心不变量：

| # | 不变量 | 说明 |
|:--|:-------|:------|
| 1 | **SSOT 原则** | `config-schema.yaml` 是唯一真理源，禁止人工维护冗余的 manifest/metadata 文件 |
| 2 | **契约驱动的模块边界** | 每个 `contract.dependencies` 表达式定义了一个跨模块耦合边界；`dependencies` 中出现的所有字段必须属于同一 bounded context，否则应拆分为独立 schema |
| 3 | **schema 优先于代码** | 所有接口签名（入参类型、返回值、默认值）必须由 `config-schema.yaml` 中的 `type`/`default` 推导，禁止在代码中手工定义与 schema 冲突的常量和类型 |
| 4 | **门禁不可绕过** | 配置值的写入必须通过 `contract.range`/`dependencies` 校验，门禁引擎是唯一的配置写入路径 |
| 5 | **运行时与配置源解耦** | 业务进程只通过 `ports.ConfigReader` 取配置值；禁止直接读取 `config-schema.yaml` |

### 4. 落地指令细化（给 subtask-b 的指令）
落地建议必须包含：

- **模块拆分**：具体到新建/修改哪个包、哪个模块（如 `internal/adapter/http/`）
- **config-schema.yaml 初始草案**：包含不少于 3 个配置字段，每个字段必须包含 `key`/`type`/`default`/`contract`（至少含 `range` 或 `dependencies`）
- **端口-适配器接口设计**：基于 `ConfigSchemaField` 推导出 `ports/` 接口方法和 `adapters/` 实现结构
- **门禁规则定义**：从 `contract` 段提取的所有 `gate_config` 条目
- **测试要求**：单元（金标准过滤/门禁逻辑）/ 集成（schema → codegen → CI 链路）/ 回归
- **涉及改动文件清单**：完整路径列表

### 5. 输出格式规范

**输出草案时：** 只给出三个方案的对比表格 + 待裁定的架构不变量 + `config-schema.yaml` 初始草案，不说多余废话。

**用户授权记录后：** 只汇报是否记录成功，不详细列出记录内容。

---

## 场景模式

本 skill 支持两种入口场景，AI 应在 Step 1 中根据用户描述自动识别并切换模式：

### 模式 A — 全新系统设计（默认）
从零推导架构。用户提供 domain + 配置字段列表即可进入标准工作流。

### 模式 B — 现有系统迁移（逆向契约推导）
当用户说 "改造 / 迁移 / 升级 一个现有系统" 时，自动进入此模式。工作流调整如下：

1. **Step 1A — 逆向契约推导**：根据用户提供的代码片段或现有配置文件（`application.yml`、`config.json` 等），反向生成 `config-schema.yaml` 草案
2. 推导规则：
   - 现有配置文件的每个 key → `fields[].key`
   - 值的类型 → `fields[].type`
   - 当前值 → `fields[].default`
   - **人工补充**：引导用户为每个字段补充 `contract`（range/dependencies/slo_impact）和 `business_desc`
3. 完成后进入 Step 2 → Step 3 → Step 4 标准流程

---

## 标准工作流

### Step 1 — 需求澄清（确认领域和配置边界）

**前置判断**：AI 自动识别用户场景 → 选择模式 A 或模式 B（见上节）。

与用户确认：
- **业务域（domain）**：如 `payment`、`auth`、`order`
- **初始配置字段列表**：至少 3 个，每个含 key/type/default/business_desc
- **跨字段约束**：如 `payment.timeout > payment.max_retry * 2`
- **SLO 影响**：如 "支付成功率"、"用户连续登录成功率 ≥99.9%"

**场景化引导**（当用户提供的字段少于 3 个时）：
> "当前只有 {n} 个配置字段，是否考虑：A. 将硬编码的常量抽离为配置（如超时时间、重试次数）以增加可观测性和运行时管控能力？B. 该模块确实无配置治理需求，可直接出方案？"

**（可选）团队上下文收集**（用于 Step 3 的决策推荐）：
- 是否需要运行时热更新？（是 → 倾向方案 B/C，否 → 方案 A 也可行）
- 团队规模（单人/小团队 → 方案 A 更友好；多团队协作 → 方案 B/C 的契约边界更有价值）
- 是否有多系统共用 Schema 的需求？（是 → 方案 C 的 Schema Registry 值得考虑）

**D. 引导原则**
- 如果用户识别的配置字段 < 3 个：提示 "当前识别出的配置扩展点较少，可选择方案 A（环境变量即可）或方案 B（预留配置扩展能力但不强求使用）"
- 如果用户识别的配置字段 ≥ 5 个且跨域依赖复杂：提示 "配置字段较多且存在依赖关系，建议采用方案 B 或 C，利用 BlessStar 门禁治理配置变更风险"

### Step 2 — 输出 config-schema.yaml 初始草案
按以下模板输出 YAML（仅关键字段，不包含下文内容）：

```yaml
# config-schema.yaml — {domain} 配置宪法
domain: {domain}
version: v1.0.0
fields:
  - key: {field_key}
    type: {I32|I64|STR|BOOL|ARR|ENUM|DURATION}
    default: "{default_value}"
    business_desc: "业务含义描述"
    contract:
      range: [{min}, {max}]
      dependencies:
        - "{expression}"
      slo_impact: "{SLO 描述}"
```

### Step 3 — 输出三方案对比表格
基于 Step 2 的 YAML，分三个方向给出架构方案。

**方案对比表格输出后，必须追加"决策推荐"子步骤**：

根据收集到的场景特征（Step 1 中确认），自动推荐最优方案：

| 场景特征 | 推荐方案 | 理由 |
|:---------|:---------|:-----|
| 配置字段 ≤5 且变更不频繁 | 方案A（极简运维） | 无需热加载，CI 静态派生即可满足，零中间件成本 |
| 配置字段 ≥10 且跨域依赖复杂 | **方案B（契约驱动）**⬅️ | 热加载 + 门禁自省，契约边界约束多团队协作冲突 |
| 配置字段 ≥20 且多系统共用 Schema | 方案C（高扩展） | Schema Registry 事件驱动确保多系统派生一致 |
| 需要运行时热更新 | 方案B 或 C | 方案A 不支持运行时变更 |
| 单人/小团队项目 | 方案A 或 B | 方案C 的中间件成本超出收益 |

### Step 4 — 用户确认 → 固化为最终草案
包含：
- 架构拓扑图（ASCII）
- 11 条架构不变量（前 5 条来自本 skill 的 "配置即宪法" 核心约束，后 6 条根据具体场景补充）
- 模块级落地指令

**固化的最终草案末尾，必须追加"下一步资产交接"动作列表**：

```text
## 下一步动作（人工执行）

1. **提交 YAML**：将 config-schema.yaml 存入业务系统仓库根目录
   cp ./output/config-schema.yaml /path/to/your-repo/

2. **运行 codegen 生成 Ports/Adapters/Mock**：
   blessstar-codegen --schema config-schema.yaml --lang go --output biz-adapters
   > **注**：`--lang go` 已验证（21 测试全部通过）；`--lang java` 尚未完整实现，JVM 语言需手动实现适配器层

3. **生成门禁配置文件**（供 GateFactory 使用）：
   blessstar-codegen --schema config-schema.yaml --lang go --gen-tests --gen-obs

4. **导入 BlessStar Admin**：将 YAML 挂载到 BlessStar 的 schema_watcher 监听目录，
   或通过 Admin API `POST /api/v1/schemas` 上传

5. **设置 CI 门禁**：将 codegen --check 加入 PR 流水线
   # .github/workflows/codegen.yml（参考）
   - run: blessstar-codegen --schema config-schema.yaml --lang go --check

6. **（现有系统迁移模式）手动验证**：对比 codegen 生成的 ports/ 与现有代码的
   接口签名是否一致，不一致处标记为"待重构"

#### 📋 产出物规范

| 产出物 | 格式 | 存放路径 | 是否必须 |
|:-------|:-----|:---------|:---------|
| 架构方案选择记录 | `.md` | `docs/architecture/ADR-<date>-<domain>.md` | ✅ 必须 |
| 架构拓扑图 | ASCII 文本 | 内嵌于 ADR | ✅ 必须 |
| 架构不变量 | 编号列表 | 内嵌于 ADR | ✅ 必须 |
| 落地建议 | 结构化章节 | 内嵌于 ADR | ✅ 必须 |
| `config-schema.yaml` | `.yaml` | 仓库根目录 | 仅配置字段 ≥3 时 |
| 门禁规则 | `.json` / `.h` | codegen 输出或 schema_watcher 挂载 | 仅方案 B/C 时 |

---

---

## 退出条件

在以下情况必须退出本 skill：
1. 用户要求编写具体业务代码（本 skill 不产生 `.go`/`.java`/`.py` 文件，只产生 `.md` 和 `.yaml`）
2. 用户要求绕过架构设计直接进入编码（应建议先完成 ADR）
3. 用户在 Step 1 中明确表示该模块无配置治理需求，且接受了引导后仍坚持（输出最小方案后退出）

---

## 与相邻 skill 的协作边界

| Skill | 输入 | 输出 | 依赖关系 |
|:------|:-----|:-----|:---------|
| `schema-first-architect`（本 skill） | 业务域 + 配置列表 / 旧配置文件 | `架构方案选择记录.md` + `config-schema.yaml` | — |
| `subtask-b`（工程落地） | `架构方案选择记录.md` 中的落地建议 | 具体代码（`.go`/`.c`/测试） | 依赖本 skill 的 ADR |
| `subtask-a`（通用架构师） | 非功能性需求 | 架构方案选择记录 | 本 skill 是 subtask-a 的 Schema-First 特化版 |
| `architect-pro`（架构优先 / 编排器） | 业务域 + 功能需求 | ADR + 可选 config-schema | **作为本 skill 的编排入口，替代 subtask-a 的场景**（当用户已备齐配置字段时，从本 skill 入口 B 进入） |

---

## 使用示例

#### 用户输入：
> "我要设计一个支付系统的配置架构，包括 payment.timeout（30s）、payment.max_retry（3 次）、payment.fee.rate（0.6%）三个配置字段，其中 timeout 必须大于 max_retry 的 2 倍。"

#### AI 响应（Step 1 后）：
"识别出 3 个配置字段，其中 `payment.timeout` 与 `payment.max_retry` 存在跨字段依赖。建议采用方案 B（契约驱动），以 BlessStar SchemaLoader 动态加载门禁规则，确保变更安全可控。当前即可输出 `config-schema.yaml` 草案。"

#### 用户确认后 → 进入 Step 2 → Step 3 → Step 4，输出完整的 ADR + config-schema.yaml + 6 步资产交接清单。|
