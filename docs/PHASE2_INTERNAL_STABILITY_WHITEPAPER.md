# BlessStar 第二阶段内部稳定白皮书（B′ 模型）

| 字段 | 值 |
|------|-----|
| **版本** | v1（草案基线，2026-05-29 用户确认 B′） |
| **范围** | 38 天计划 **第 1～17 天** 已确认架构与工程验收口径 |
| **读者** | 主任务商用前评审、子任务 A/B、内部研发 |
| **对外** | 须脱敏另版；本文含内部路径与例外表 |

---

## 1. 范围与「稳定」口径

### 1.1 第二阶段目标（摘要）

在 **BlessStar Config JSON v1** 主链上，完成分层架构、attach/reload/Kernel 主路径、安全与持久化、契约治理（D 方案）及 **L1/L2/L3 测试分工**（XVII-TEST-ENV）。

### 1.2 第 18 天「内核核心稳定」判定（工程口径）

**满足即视为第二阶段评审通过（工程侧）：**

- 本文 **§4 记分卡 A 节** 全部 **Must-PASS**；
- **不**要求 ASan/Valgrind 全量绿（**第 19 天**主题）；
- **不**要求 VP-5、adapter 全量瘦身、L2 Staging 全量验收。

### 1.3 B′ 模型

| 层级 | 内容 |
|------|------|
| **硬阻塞** | 契约记分卡（方案 B） |
| **交付物** | 本白皮书 + 《第二阶段总结报告》`docs/PHASE2_STAGE2_SUMMARY_REPORT.md` |
| **进第 19 天前建议（P0，非第 18 天阻塞）** | R8-03 收敛声明（§5.1）+ hermetic IO 单测迁移计划（§5.2） |
| **排除** | `IMPL-17-D-31`（VP-5）、`C-FN-*`/`C-CF-*` 全量深化 |

---

## 2. 架构决策摘要（第 1～17 天）

| 域 | 决策要点 | 权威锚点 |
|----|----------|----------|
| 分层 | L1～L4 + app；`kernel → adapter` 单向依赖 | 《架构方案选择记录》第 2 天；`C-IX-1` |
| 配置主链 | 仅 **JSON v1**；`bs_adapter_parser_parse_bytes` | 第 9 天 A′-OPT |
| 业务语义 | **app/sdk**；adapter 编排 io/parse/gate/attach/watch | `C-IX-1`、`C-IX-3` |
| 厂商格式 | 解析 **仅** `app/sdk`（含 `vendor/`） | `C-IX-7`、`C-FN-3/4` |
| 内核执行 | **Kernel + Pipeline** 进 AttachContext；reload 顺序 gate→execute→persist→sync | **XVII-KERNEL** |
| 会话/状态 | 单 AttachContext；CM **XVII-CM**；freeze **XVII-ATTACH/NOTIF** | 第 17 天 T6～T8 |
| 持久化/观察 | 第 14/15 天原子 persist / watch 最小模型 | ATOM-XIV、day15 |
| 治理 | 契约 + gate + lock plan；day17 **24/24** | D-XVII、**XVII-TEST-ENV** |

---

## 3. 纯度原则

### 3.1 契约裁切

- **C-IX-1**：业务语义在 app，adapter 保持通用。
- **C-IX-2**：Kernel 只消费通用 IR，不读业务配置原文。
- **C-IX-3**：adapter 负责 io/parse/gate/attach/watch 编排。
- **C-IX-7**：厂商格式不得进入 `adapter/parser`。

### 3.2 行业对照（模式级）

| 参考系统 | 模式 | BlessStar 映射 |
|----------|------|----------------|
| Kubernetes Kubelet | 节点运行时 vs 业务 Pod | adapter/runtime vs app 策略 |
| Meson | 解释器 vs 用户项目 | parser v1 vs `ScenarioPolicy` |
| ZooKeeper | Server 核心 vs 客户端 API | kernel/registry vs attach 会话 |
| Spring Cloud | Bootstrap vs Application | bootstrap/freeze vs app normalize |

---

## 4. 稳定性记分卡

### 4.1 Must-PASS（第 18 天硬阻塞）

| ID | 检查项 | 门禁 / 证据 | 状态 |
|----|--------|-------------|------|
| SC-18-01 | 业务在 app | `C-IX-1`、`GATE-ARCH-INCLUDE-BOUNDARY` | ✅ |
| SC-18-02 | 内核仅 IR | `C-IX-2`、integration 主链 | ✅ |
| SC-18-03 | adapter 编排 | `C-IX-3`、`bs_test_app_sdk_contract` | ✅ |
| SC-18-04 | 厂商仅在 app | `C-IX-7`、`GATE-VENDOR-PARSE-BOUNDARY` | ✅ |
| SC-18-05 | 契约 lock plan | `contract_validate_instances.py`、`contract_compile.py` | ✅ |
| SC-18-06 | L1 CI | `ctest -L day17`（含 `--through-stage ci`） | ✅ 24/24 |
| SC-18-07 | L1 gate 范围 | `C-TST-GATE-SCOPE-1` | ✅ |
| SC-18-08 | 三集成 hermetic | `C-TST-HERM-1`、三集成测 + `test_temp_dir.h` | ✅ |
| SC-18-09 | Kernel 主链 | `bs_test_kernel_runtime`、reload IR 集成 | ✅ |
| SC-18-10 | persist/watch | day14/day15 回归（记录已闭合） | ✅ |

**复核命令（本地）：**

```bash
cmake -S . -B build_ci_test -DBLESSSTAR_BUILD_TESTS=ON
cmake --build build_ci_test --config Release
ctest --test-dir build_ci_test -C Release -L day17 --output-on-failure
python tools/scripts/contracts/contract_validate_instances.py
python tools/scripts/contracts/contract_compile.py
```

### 4.2 建议项（进第 19 天前 P0 · 非第 18 天阻塞）

| ID | 项 | 状态 | 参见 |
|----|-----|------|------|
| P0-R8-03 | R8-03 收敛声明 | ✅ 已声明（§5.1） | `check_includes.py` |
| P0-HERM-IO | IO 单测 hermetic 迁移 | ✅ 已闭合（2026-06-01） | §5.2；《架构方案选择记录》变更 **18.2** |

---

## 5. 进第 19 天前建议项（P0）

### 5.1 P0-R8-03：R8-03 收敛声明

**裁定回顾（第 8 天）**：**A+B+C** — Include 门禁 + 测试 mock + `kernel/common/test_support`。

**MVP 操作定义（R8-03 闭合条件）：**

1. `python tools/purity/check_includes.py --repo-root .` 输出 **`Include gate OK`**（0 违规）。
2. `kernel/**/test/**` 不得 `#include` `adapter/` 路径头文件。
3. 生产 `adapter/` 对 `ConfigManager.h` 的 include 仅限脚本白名单（如 `attach_config.cpp`）。

**验证记录（2026-05-29）：**

```
Include gate OK
```

**残余风险（登记，不阻塞第 18 天）：** 逻辑/链接层耦合仍可能存在于行为面；与 **第 19～20 天** ASan/TSan/并发主题分开评估。

**跨日索引：** 《架构方案选择记录》**架构短板跨日总索引** 中 **R8-03** 已由 🟡 更新为 ✅（用户授权，2026-05-29）。

### 5.2 P0-HERM-IO：hermetic IO 单测迁移计划

**目标：** 并行 `ctest -L regression` 时，IO 单测不依赖 build 目录 **固定文件名**；对齐 **C-ST-10** / **C-TST-HERM-1**。

**已完成（第 17 天）：** `ReloadConfigJsonIntegrationTest`、`Day8AttachFullIntegrationTest`、`AppVendorReloadIntegrationTest` + `check_test_tempdir_unique.py` 强制三文件。

| 波次 | 测例 | 动作 |
|------|------|------|
| W1 | `bs_test_io_reload_batch` | `test_temp_dir.h` + 唯一子目录 manifest |
| W2 | `bs_test_io_attach_pipeline`、`bs_test_io_registry_phase` | 同上；视需要 `RESOURCE_LOCK attach_hermetic_temp` |
| W3 | 其余 `bs_test_io_*`（facade/local/boundary/timeout/status_table 等） | 清除 `check_test_tempdir_unique.py` BAD_PATTERNS 命中 |
| W4 | 门禁 | 扩展 `check_test_tempdir_unique.py` 覆盖策略与 W1～W3 同步 |

**P0 完成定义：** `check_test_tempdir_unique.py` PASS 且无未登记固定名；regression 无 IO flaky。

**状态（2026-06-01）：** W1～W4 已落地；`ctest -R bs_test_io_` 冒烟 **14/14**；详见《架构方案选择记录》第 18 天 § **阶段 P0** / 变更 **18.2**。

---

## 6. 二期与刻意未定义

| 项 | 说明 |
|----|------|
| **IMPL-17-D-31（VP-5）** | 用友/金蝶 parser、插件动态加载、App 内嵌 Python |
| **C-FN-* / C-CF-* 深化** | 按后续日程扩充 |
| **adapter 瘦身二期** | 见第 17 天遗留 |
| **L2 全量 Staging** | `ops/acceptance/staging/run_acceptance.py` 无 dry-run，由 CD 执行 |
| **多环境 lock plan 合并** | L1/L2/L3 分工已入库；全量跨环境 lock 仍二期 |

---

## 7. 附录

| 文档 | 路径 |
|------|------|
| 架构方案选择记录 | `架构方案选择记录.md` |
| 测试环境政策 | `docs/BLESSSTAR_TEST_ENVIRONMENT_POLICY.md` |
| 契约索引 | `docs/contracts/index.json` |
| Gate 注册表 | `docs/gates/gate_registry.json` |
| Lock plan | `docs/reports/contract_plan.lock.json` |
| 工程变更台账 | `项目修改记录.md` |
| 第二阶段总结（简版） | `docs/PHASE2_STAGE2_SUMMARY_REPORT.md` |

---

*维护：子任务 B；模型 B′ 由用户 2026-05-29 确认。*
