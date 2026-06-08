# DAY17: Style Contract (Fifth Type)

本文对应 `IMPL-17-01`，定义第五类契约：**风格契约（`C-ST-*`）**。

## 1. 与第16天四类契约的关系

| 类型 | ID 前缀 | 优先级（冲突裁决） |
|------|---------|-------------------|
| 架构契约 | `C-IX-*` | 最高 |
| 对接契约 | `C-IX-4/5/8` 等 | 第二 |
| **风格契约** | **`C-ST-*`** | **第三（新增）** |
| 功能契约 | `C-FN-*`（占位） | 第四 |
| 配置契约 | `C-CF-*`（占位） | 第五 |

**C-IX-6′（第17天）**：架构 > 对接 > 风格 > 功能 > 配置（取代第16天四字链，第16天历史条文保留）。

## 2. 风格契约模板

| 字段 | 含义 |
|------|------|
| `contract_id` | `C-ST-n` |
| `name` | 英文短名 |
| `scope` | kernel / adapter / app / test / cmake / docs |
| `rule` | 可执行规则描述 |
| `verify` | lint / doc-check / cmake 脚本 ID |
| `priority` | 固定 `style` |
| `status` | draft / active / deprecated |

## 3. 机器可读权威源

- `docs/contracts/style.contracts.json`（`v1`，`C-ST-1`～`C-ST-13`）

## 4. clang-format 两批范围（C-ST-5）

1. **批①**：`kernel/**/include/**`、`adapter/include/**`、`app/**/include/**`
2. **批②**：`kernel/**`、`adapter/**`、`app/sdk/**` 下 `src/` 及同层 `*.c`/`*.cpp`

命令入口：`tools/scripts/format/run_clang_format_check.py --batch include|src`
