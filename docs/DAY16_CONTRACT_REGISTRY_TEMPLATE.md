# DAY16: Contract Registry Template

本文对应 `IMPL-16-02B`，定义 Contract Registry 字段与示例。

## 1. 字段定义

| 字段 | 含义 |
|------|------|
| `id` | 契约唯一编号（如 `C-IX-1`） |
| `version` | 契约版本（`v1`/`v1.1`） |
| `scope` | 生效范围（kernel/adapter/app/test/docs） |
| `priority` | 冲突优先级（**C-IX-6′**：架构 > 对接 > **风格** > 功能 > 配置） |
| `verify` | 验证项（lint/test/benchmark/doc-check） |
| `implementations` | 机器可执行绑定（见 `docs/contracts/index.json`；每条约一条 gate 的 `entry` + `entry_kind`） |
| `deprecate` | 废弃策略（替代条文、迁移窗口、失效日期） |
| `owner` | 维护责任人/角色 |
| `status` | draft/active/deprecated |

## 2. 表格模板

| id | version | scope | priority | verify | deprecate | owner | status |
|----|---------|-------|----------|--------|-----------|-------|--------|
| C-IX-1 | v1 | app+adapter | 架构 | doc-check + lint | - | main-task | active |
| C-IX-4 | v1 | adapter | 对接 | unit/integration | - | subtask-b-eng | active |
| C-IX-6-prime | v1 | docs+kernel+adapter+app | 架构 | doc-check:contract-files | day16 C-IX-6 four-tier | main-task | active |
| C-ST-1 | v1 | kernel+adapter+app | style | lint:public-api-prefix | - | subtask-b-eng | active |
| C-ST-5 | v1 | kernel+adapter+app | style | lint:clang-format-dry-run | - | subtask-b-eng | active |
| C-ST-9 | v1 | test+cmake | style | cmake:ctest-label-check | - | subtask-b-eng | active |
| C-ST-12 | v1 | docs | style | doc-check:contract-files | - | subtask-b-eng | active |

## 3. 使用规则

- 未填写 `verify` 的契约不得进入 `active`。
- 变更 `version` 必须写明迁移说明与兼容窗口。
- `deprecate` 仅用于已存在替代条文的契约。
