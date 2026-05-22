# 第一阶段回归测试报告（IMPL-08-16）

> **生成角色**：子任务 B  
> **基线日期**：2026-05-18  
> **命令环境**：Windows · MSVC · `cmake --build build --config Debug`  
> **全量门禁**：`ctest --test-dir build -C Debug --output-on-failure`

## 1. 标签维度（CTest）

| 标签 | 含义 | 典型命令 |
|------|------|----------|
| `unit` | 全部可执行测例（默认） | `ctest --test-dir build -C Debug` |
| `regression` | 已实现链路的主题回归 | `ctest -C Debug -L regression` |
| `integration` | 跨模块端到端链 | `ctest -C Debug -L integration` |
| `attach` | Registry attach / bootstrap / 插件 | `ctest -C Debug -L attach` |
| `registry` | PathRegistry / Facade / Hub | `ctest -C Debug -L registry` |
| `io` | IoFacade / Provider | `ctest -C Debug -L io` |
| `day7` | 第7天错误/日志/编排专测 | `ctest -C Debug -L day7` |
| `day8` | 第8天 AttachContext / 插件 / 复盘相关 | `ctest -C Debug -L day8` |
| `day9` | 第9天 Config JSON v1 parse + reload 闭合 | `ctest -C Debug -L day9` |
| `day10` | 第10天模型 C + `BS_JSON_MAX_*` 边界 | `ctest -C Debug -L day10` |
| `day11` | 第11天 SEC-IX 安全矩阵 | `ctest -C Debug -L day11` |
| `day12` | 第12天 attach 持久化 / CAS / per_batch | `ctest -C Debug -L day12` |
| `day13` | 第13天安全审计（P0/P1 边界 + manifest/session） | `ctest -C Debug -L day13` |
| `parser` | JSON lexer/parser + `bs_config_parse_bytes` | `ctest -C Debug -L parser` |
| `comprehensive` | 慢速/大范围 common 测 | `ctest -C Debug -L comprehensive` |

## 2. 已实现链路 ↔ 测例矩阵

### 2.1 Attach / Registry（第5～8天）

| 链路 | 测例 | 标签 |
|------|------|------|
| PathRegistry 相位 / freeze | `bs_test_path_registry` | registry; day8; regression |
| Facade / Hub / Guard | `bs_test_registry_facade` 等 | registry; regression |
| bootstrap + 自定义 P2 插件 + IR gate | `bs_test_attach_pipeline_registry` | attach; integration; regression |
| 契约（manifest、相位、bind 守卫） | `bs_test_registry_attach_contract` | attach; regression |
| bootstrap 冒烟 | `bs_test_registry_integration` | attach; registry; regression |
| AttachContext begin→io→freeze | `bs_test_attach_context_bootstrap` | attach; day8; regression |
| **插件 loader + freeze** | `bs_test_plugin_loader_attach` | attach; day8; regression |
| freeze → EventBus notifier | `bs_test_attach_freeze_eventbus_integration` | attach; day8; integration; regression |
| **插件 log 域 + format** | `bs_test_plugin_log_domains_attach_integration` | attach; day8; integration; regression |
| **day8 全链：插件+IO+format+reload+Report** | `bs_test_day8_attach_full_integration` | attach; day8; io; integration; regression |
| orch-reload Registry factory | `bs_test_plugin_orch_reload_registry` | attach; day8; regression |
| attach_plugins.yaml 解析 | `bs_test_attach_manifest_yaml` | attach; day8; regression |
| ir_requirements_ref 子集 | `bs_test_plugin_ir_requirements` | attach; day8; regression |

### 2.2 IO（第6天）

| 链路 | 测例 | 标签 |
|------|------|------|
| IoFacade + registry resolve | `bs_test_io_facade` | io; regression |
| Local provider / boundary / timeout | `bs_test_io_local_provider*` | io; regression |
| bootstrap→freeze→read | `bs_test_io_attach_pipeline` | io; attach; integration; regression |
| freeze 后写拒绝 | `bs_test_io_registry_phase` | io; registry; regression |
| bootstrap 仅 io 插件 | `bs_test_registry_bootstrap_io` | io; attach; regression |

### 2.3 错误 / 日志 / 编排（第7天）

| 链路 | 测例 | 标签 |
|------|------|------|
| BsStatus / format | `bs_test_status` | day7; day8; registry; regression |
| Log ring + mock bus | `bs_test_log` / `bs_test_log_audit` | day7; regression |
| status 域 freeze | `bs_test_registry_status_domain` | day7; registry; regression |
| Reload 默认 gate（v1 JSON parse + ir_gate） | `bs_test_reload_gate_default` | day7; day9; parser; io; regression |
| Reload + Report | `bs_test_reload_report` | day7; io; regression |
| **file:// JSON → read → parse → gate** | `bs_test_reload_config_json_integration` | day9; parser; io; attach; integration; regression |
| Config parse 主链 | `bs_test_config_parse` / `bs_test_json_*` | day9; parser; regression |
| **模型 C + 1MiB/4096 边界** | `bs_test_config_parse_boundary` | day10; parser; regression |
| LOG-VII-10 attach 守卫 | `bs_test_reload_attach_guard` | day7; attach; regression |
| **attach 持久化 / CAS / scheme** | `bs_test_attach_resilience` | attach; day12; regression |
| **安全审计 P0/P1（instructions/manual/manifest）** | `bs_test_config_parse_security_audit` | day13; parser; regression |
| **集成：bootstrap→read→reload→EventBus→重入** | `bs_test_reload_default_gate_report_eventbus_reentry_integration` | day7; integration; regression |
| EventBus 重入 + IO | `bs_test_reentrancy_io` | day7; attach; io; regression |
| io/registry status 表 | `bs_test_io_status_table` | day7; io; registry; regression |
| Result 映射 | `bs_test_result_status_map` | day7; regression |

### 2.4 刻意未纳入（链路未闭合）

| 项 | 原因 |
|----|------|
| WIRE-07 CLI attach→reload | 二期 08-23 |
| orch-reload Registry 扩展点 | ✅ 08-17 阶段 3 · `bs_test_plugin_orch_reload_registry` |
| ConfigManager 进 bootstrap | R8-07 C 二期 |
| Parser 字节→IR（WIRE 全链） | IMPL-06-03 **M3 最小闭合** · `bs_test_reload_config_json_integration` |

## 3. 推荐回归命令（本地 / CI）

```bash
# 全量（与 CI 一致，须指定 -C Debug 于多配置生成器）
ctest --test-dir build -C Debug --output-on-failure

# 主题回归
ctest --test-dir build -C Debug -L regression --output-on-failure

# 集成链
ctest --test-dir build -C Debug -L integration --output-on-failure

# 第8天增量
ctest --test-dir build -C Debug -L day8 --output-on-failure

# 第9天 parser + reload 闭合
ctest --test-dir build -C Debug -L day9 --output-on-failure
ctest --test-dir build -C Debug -L parser --output-on-failure

# 第10天边界（模型 C / 1MiB / 串长）
ctest --test-dir build -C Debug -L day10 --output-on-failure

# 第11天安全（UTF-8 / depth / 重复键）
ctest --test-dir build -C Debug -L day11 --output-on-failure

# 第12天 attach 韧性
ctest --test-dir build -C Debug -L day12 --output-on-failure

# 第13天安全审计
ctest --test-dir build -C Debug -L day13 --output-on-failure
```

## 4. 结果摘要（2026-05-20 · Windows Release）

| 维度 | 用例数 | 结果 |
|------|--------|------|
| 全量（`ctest -C Release`） | **65** | **65/65** |
| `-L regression` | **44** | **44/44** |
| `-L integration` | **7** | **7/7** |
| `-L day9` | **5** | **5/5** |
| `-L day10` | **1** | **1/1** |
| `-L parser` | **7** | **7/7** |
| `-L day11` | **1** | **1/1** |
| `-L day12` | **1** | **1/1** |
| `-L day13` | **1** | **1/1** |
| `-L day8` | **10** | **10/10** |
| `-L day7` | **12** | **12/12** |
| `-L attach` | **21** | **21/21** |
| `check_includes.py` / `check_freeze_order.py` | — | **OK** |

> MSVC 多配置生成器须加 `-C Release` 或 `-C Debug`。Release 测例通过 `cmake/Tests.cmake` 的 `/UNDEBUG` 保留 `assert` 语义（避免 NDEBUG 下假绿）。  
> 第 9 天新增：`bs_test_json_lexer`、`bs_test_json_parser`、`bs_test_config_parse`、`bs_test_reload_config_json_integration`。  
> 第 10 天新增：`bs_test_config_parse_boundary`（`-L day10`）；厂外 `money_normalize.py` CI smoke 见 `docs/DAY10_PARSER_COVERAGE.md`。  
> 第 11 天新增：`bs_test_config_parse_security`（`-L day11`）；矩阵见 `docs/DAY11_SECURITY_TEST_MATRIX.md`。  
> 第 12 天新增：`bs_test_attach_resilience`（`-L day12`）；矩阵见 `docs/DAY12_ATTACH_RESILIENCE_MATRIX.md`。  
> 第 13 天新增：`bs_test_config_parse_security_audit`（`-L day13`）；审计见 `docs/DAY13_SECURITY_AUDIT.md`。

## 4.1 Sanitizer 门禁（Linux Clang · CI）

| 维度 | 说明 |
|------|------|
| Workflow | `.github/workflows/ci.yml` · job `sanitizer` |
| 配置 | `ubuntu-latest` · Clang · `-fsanitize=address,undefined` |
| 状态 | **blocking**；以 GitHub Actions 首次/持续绿为准（本地 Windows Release 不能代替） |

## 5. 相关文档

- `docs/PHASE1_ARCHITECTURE_REVIEW.md`  
- `docs/IO-II-REGISTRY-COUPLING.md`  
- `adapter/manifest/attach_plugins.yaml`
