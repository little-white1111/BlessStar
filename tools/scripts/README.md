# `tools/scripts` 目录说明

按功能划分子目录；仓库根目录通过 `lib/repo_paths.py` 的 `repo_root()` 解析。

| 子目录 | 用途 |
|--------|------|
| `contracts/` | D 方案：契约编译、lock plan、gate runner |
| `gates/` | 单条 gate 可执行检查（由 `docs/gates/gate_registry.json` 或 CTest 调用） |
| `format/` | 代码格式（clang-format） |
| `maintenance/` | 一次性/维护类脚本 |
| `perf/` | 性能基线脚本 |
| `test/` | 测试报告脚本（非 blocking，例如 CTest label coverage 汇总） |
| `lib/` | 公共路径与扫描辅助（`repo_paths.py`） |

## 契约实例字段（D 方案）

每条 `docs/contracts/<type>/*.v1.json` 除 `rule`（人类条文）外，必须包含机器可执行的 `implementations[]`：

| 字段 | 说明 |
|------|------|
| `gate_id` | 与 `gate_refs` 一致 |
| `runner` | `python` / `ctest`，须与 `gate_registry` 一致 |
| `entry` | `entry_kind=script` 时为仓库内脚本路径；`command` 时为完整 ctest 命令 |
| `entry_kind` | `script` 或 `command` |

仓库级约定见 `docs/contracts/index.json`（`script_layout`、`add_gate_workflow`、`repository_layout`）。

## 常用命令

```bash
python tools/scripts/contracts/contract_validate_instances.py
python tools/scripts/contracts/contract_compile.py
python tools/scripts/contracts/contract_gate_runner.py
python tools/scripts/test/collect_coverage.py --json-out docs/reports/ctest-label-coverage.json
```
