# Testing environment contracts（测试设计契约子库）

本目录是 BlessStar **开发与生产环境测试分工** 的契约根（`contract_roots.testing`）。

与 `architecture/`、`integration/` 等并列，但 **仅约束测试资产归属与执行环境**，不重复业务不变量条文。

## 三层模型（L1 / L2 / L3）

| 层级 | 环境 | 触发 | 权威清单 |
|------|------|------|----------|
| **L1** | Dev CI（`build_ci_test` + 仓库内 ctest） | PR / `ctest -L day17` | `tier_assignments.json` → `L1_dev_ci.ctest_names` |
| **L2** | Staging（与生产同构） | CD 部署 Staging 后 | `ops/acceptance/staging/scenarios.v1.json` |
| **L3** | Production | 部署 Prod 后 / 定时合成 | `ops/smoke/prod/scenarios.v1.json` |

## 契约实例（`*.v1.json`）

| ID | 要点 |
|----|------|
| **C-TST-POL-1** | 三层政策：L1 禁止进 Prod；L2 阻断晋升；L3 仅冒烟 |
| **C-TST-GATE-SCOPE-1** | L1 `contract_gate_runner` 止于 `ci`；L2 批量 label 不得进 day17 ctest |
| **C-TST-L1-1** | Dev 快速门禁（day17 + `--through-stage ci`） |
| **C-TST-L2-1** | Staging 验收剧本必过（含 `integration` / `day14` 批量 label） |
| **C-TST-L3-1** | Prod 仅 smoke，禁止全量 regression |
| **C-TST-HERM-1** | 文件型 adapter/app 测例须 `test_temp_dir.h`；`GATE-TEST-HERM` 禁止固定 `bs_*` 路径（P0-HERM-IO） |
| **C-TST-ENV-1** | Staging 与 Prod 拓扑/路径策略同构 |
| **C-TST-MEM-1** | Day19 内存压测阈值索引（T19.12-C rule-only；无 blocking smoke gate） |

## 门禁

见 `docs/gates/gate_registry.json`：

- `GATE-TEST-TIER-ASSIGNMENT`：每个 CTest 名唯一归属 L1
- `GATE-TEST-L1-GATE-SCOPE`：`check_contract_gate_runner_l1_scope.py`（CMake/registry/tier 一致）
- `GATE-STAGING-ACCEPTANCE`：L2 剧本
- `GATE-PROD-SMOKE`：L3 剧本（支持 `--dry-run` 本地校验）

人类可读政策：`docs/BLESSSTAR_TEST_ENVIRONMENT_POLICY.md`
