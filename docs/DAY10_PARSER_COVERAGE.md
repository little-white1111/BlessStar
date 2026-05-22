# Day10 · `adapter/parser` 测试覆盖说明（裁定点 3-Ⅰ）

> **范围**：仅 `adapter/parser` 生产库 `bs_adapter_parser`（`json_lexer` / `json_parser` / `config_parse` / `config_v1_*`）。  
> **门禁**：仅报告，**CI 不设覆盖率硬门槛**；与 `ctest -L day10` / `-L parser` 同批维护。

## 1. 采集方式

当前环境以 **CTest 行为覆盖矩阵** 为主（未启用 gcov/lcov）。推荐命令：

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release -L day10 --output-on-failure
ctest --test-dir build -C Release -L parser --output-on-failure
ctest --test-dir build -C Release --output-on-failure
```

| 标签 | 测例 | 2026-05-20 Release |
|------|------|---------------------|
| `day10` | `bs_test_config_parse_boundary` | 1/1 |
| `parser` | json_lexer / json_parser / config_parse / boundary / reload_config_json_integration | 6/6 |
| 全量 | 全部 `unit` | **62/62** |

## 2. 源文件 ↔ 测例映射

| 源模块 | 路径 | 覆盖测例 | 说明 |
|--------|------|----------|------|
| JSON Lexer | `adapter/parser/src/json/json_lexer.c` | `bs_test_json_lexer` | token、空白、坏输入 |
| JSON Parser v1 | `adapter/parser/src/json/json_parser.c` | `bs_test_json_parser`、`bs_test_config_parse`、`bs_test_config_parse_boundary` | AST、schema 键、串长贴线 |
| Config parse 编排 | `adapter/parser/src/config/config_parse.c` | `bs_test_config_parse`、`bs_test_config_parse_boundary`、`bs_test_reload_config_json_integration` | 版本门、IR 生成 |
| Config v1 AST | `adapter/parser/src/config/config_v1_ast.c` | 同上 | 销毁链 |
| Config v1 IR | `adapter/parser/src/config/config_v1_ir.c` | 同上 + reload gate | metadata → `IRMetadata` |
| Status 域 | `adapter/parser/src/config/config_parse_status.c` | 边界测 `domain_id=60` | `BsStatus` 编码 |

### 2.1 Day10 边界矩阵（`ConfigParseBoundaryTest`）

| 用例 | 维度 | 预期 |
|------|------|------|
| `test_model_c_parse_and_gate` | 模型 C（400×15 键）+ `verify_instructions` | `BS_STATUS_OK` |
| `test_string_length_4095_ok` | `BS_JSON_MAX_STRING_LEN` 贴线 | OK |
| `test_string_length_4097_fail` | 串长超限 | 非 OK |
| `test_input_over_1mb_fail` | `BS_JSON_MAX_INPUT_BYTES` 超限（5500×15） | 非 OK |
| `test_input_near_1mb_ok` | 近 1MiB 内正例 | OK |
| `test_empty_metadata_value_ok` | 空 metadata 串 | OK |
| `test_truncated_json_fail` | 截断 JSON | 非 OK |
| `test_model_a_light_optional` | 模型 A 轻量（400×2 键） | OK |

## 3. 未纳入 / 骨架路径（已知盲区）

| 路径 | 原因 |
|------|------|
| `adapter/parser/src/parser.c` | 早期 skeleton，**非** MVP `reload_gate_default` 主链 |
| `schema_registry.c`、`ir_generator.c`、`format_adapter.c`、`meta_executor.c`、`ast_node.c` | 未接 day9/day10 主链；3-Ⅰ 刻意不扩 scope |
| `BS_JSON_MAX_DEPTH`（32） | 头文件已定义，`json_parser.c` **未**递增 `ParseCtx.depth`  enforcement（**BOUND-G-01**，见 `项目修改记录.md` 10.1） |

## 4. 厂外脚本（非 parser 库覆盖率）

| 脚本 | 验证方式 |
|------|----------|
| `tools/normalize/examples/identity_normalize.py` | CI smoke（第9天延续） |
| `tools/normalize/examples/money_normalize.py` | CI：正例 exit 0、负例 exit 1 |

金额/税率语义 **不在** C parser 内验证（裁定点 **2-A′**）。

## 5. 后续（可选）

- 启用 gcov 时对 `bs_adapter_parser` 单独 `lcov --extract`；排除 `tools/normalize/**`。
- 第11天安全/畸形矩阵单独增测，不并入本表硬门槛。
