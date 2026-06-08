# BlessStar 测试环境分工政策（L1 / L2 / L3）

契约权威源：`docs/contracts/testing/`（测试设计契约子库）  
机器清单：`docs/contracts/testing/tier_assignments.json`

## 1. 三层定义

| 层级 | 环境 | 何时跑 | 工具 | 阻断 |
|------|------|--------|------|------|
| **L1** | 开发（Dev CI） | 每次 PR / main | `ctest` + `contract_gate_runner`（day17） | 合码 |
| **L2** | 预发（Staging） | 部署 Staging 成功后 | `ops/acceptance/staging/run_acceptance.py` | 晋升 Prod |
| **L3** | 生产（Prod） | 部署 Prod 后 / 定时 | `ops/smoke/prod/run_smoke.py` | 告警/回滚 |

**禁止**：在 Production 执行 `ctest -L regression`、`contract_gate_runner` 全量回归、或任何破坏性 persist 压测。

## 2. L1（开发）范围

- **全部已注册 CTest** 列入 `tier_assignments.json` → `L1_dev_ci.ctest_names`（含 `bs_test_day17_test_environment_tier_check`）。
- **`bs_test_day17_contract_gate_runner`** 使用 `--through-stage ci`，不在 PR 内执行 L2/L3 全量 `ctest`（避免 Staging 剧本中的 `integration` 标签跑满超时）。
- **`C-TST-GATE-SCOPE-1`** / `bs_test_day17_test_l1_gate_scope_check`：机器校验上述边界（CMake、`gate_registry`、`tier_assignments`、L2 批量 label 与 day17 互斥）。
- **推荐命令**：
  ```bash
  cmake -S . -B build_ci_test -DBLESSSTAR_BUILD_TESTS=ON
  cmake --build build_ci_test --config Release
  ctest --test-dir build_ci_test -C Release -L day17 --output-on-failure
  python tools/scripts/gates/check_test_environment_tiers.py
  ```
- **Hermetic**：文件型 reload 集成测须 `adapter/test/support/test_temp_dir.h`（**C-TST-HERM-1**）。

## 3. L2（Staging）范围

- 剧本：`ops/acceptance/staging/scenarios.v1.json`（当前 6 条，覆盖 reload / day8 / vendor / day14 / runtime / integration）。
- **环境变量**（建议）：
  - `BS_STAGING_ROOT`：独立 manifest 根（与 `build_ci_test` 分离）
  - `BS_STAGING_BUILD_DIR`：指向已部署二进制对应的构建目录（默认可复用 `build_ci_test`）
- **命令**：
  ```bash
  export BS_STAGING_ROOT=/var/lib/blessstar/staging
  python ops/acceptance/staging/run_acceptance.py
  ```

## 4. L3（Production）范围

- 剧本：`ops/smoke/prod/scenarios.v1.json`（短、非破坏性）。
- **环境变量**：
  - `BS_PROD_BLESSSTAR_HOME`：安装根
  - `BS_PROD_MANIFEST_ROOT`：租户 manifest 根
- **本地校验剧本**（不触生产）：
  ```bash
  python ops/smoke/prod/run_smoke.py --dry-run
  ```

## 5. Staging 与 Prod 同构（C-TST-ENV-1）

- 同一 OS 族（生产为 Linux 时，Staging 必须 Linux）。
- **每租户/每实例独立 manifest 路径**；禁止多进程共写同一 manifest 文件。
- 插件清单与生产一致。

## 6. 与现有契约的关系

| 现有 | 关系 |
|------|------|
| C-IX / C-FN / C-ST | 业务与风格不变量；在 **L1** 通过 ctest/gate 证明 |
| C-TST-* | 仅约束 **在哪跑、跑哪套** |
| `contract_gate_runner` | 归属 **L1**；其内 `GATE-REGRESSION` 不得映射到 Prod |

## 7. 演进

- 新增 CTest：必须同步 `tier_assignments.json`（`check_test_environment_tiers.py` 会失败）。
- 新增 Staging 剧本：只改 `scenarios.v1.json`，不增加 L1 数量。
- Benchmark（`bs_benchmark_*`）：**out_of_band**，在 Staging 性能任务手动执行。
