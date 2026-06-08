# DAY17: Contract Verification Matrix

本文对应 `IMPL-17-04`，扩展第16天矩阵，覆盖 **C-IX-6′** 与 **C-ST-1～13**。

| 契约条文 | 验证类型 | 入口 |
|----------|----------|------|
| C-IX-6′ 五层优先级 | doc-check | `architecture.contracts.json` + Registry 模板 |
| C-ST-1 对外 C API 前缀 | lint | `bs_test_day17_public_api_prefix_check` |
| C-ST-2 C++ 命名空间 | lint | `bs_test_day17_namespace_boundary_check` |
| C-ST-3 测试 target 命名 | cmake | `bs_test_day17_target_name_check` |
| C-ST-4 契约 ID 前缀 | doc-check | `bs_test_day17_contract_id_prefix_check` |
| C-ST-5 clang-format 两批 | lint | `bs_test_day17_clang_format_include` / `_src` |
| C-ST-6 include 顺序 | lint | warning-first（`status: draft`） |
| C-ST-7 对外头文件说明块 | doc-check | `bs_test_day17_public_header_contract_block_check` |
| C-ST-8 注释与契约一致 | doc-check | draft，人工为主 |
| C-ST-9 ctest 标签 | cmake | `bs_test_day17_ctest_labels_check` |
| C-ST-10 并行 temp 目录 | lint | `bs_test_day17_test_tempdir_unique_check` |
| C-ST-11 benchmark 标签 | cmake | `bs_test_day17_ctest_labels_check` |
| C-ST-12 风格 JSON + Registry | doc-check | `bs_test_day17_contract_files_check` |
| C-ST-13 可观察语义 | test | `-L integration`、`-L regression` |

## 结构化契约文件

- `docs/contracts/architecture.contracts.json`（含 `C-IX-6-prime`）
- `docs/contracts/integration.contracts.json`
- `docs/contracts/style.contracts.json`

## 推荐命令（第17天）

```powershell
ctest --test-dir build_ci_test -C Release -L day17 --output-on-failure
ctest --test-dir build_ci_test -C Release -L day16 --output-on-failure
ctest --test-dir build_ci_test -C Release -L day15 --output-on-failure
ctest --test-dir build_ci_test -C Release -L day14 --output-on-failure
ctest --test-dir build_ci_test -C Release -L integration --output-on-failure
ctest --test-dir build_ci_test -C Release -L regression --output-on-failure
```
