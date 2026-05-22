# Day11 安全测例矩阵（SEC-IX）

| ID | 类别 | 夹具 / 测例 | 期望 | 证据 |
|----|------|-------------|------|------|
| SEC-11-01 | 基线 | `bs_test_build_security_minimal_ok` | `bs_config_parse_bytes` OK | `ConfigParseSecurityTest` |
| SEC-11-02 | UTF-8 | `\uD800` surrogate | 失败 + `error_line/column > 0` | `test_utf8_surrogate_rejected` |
| SEC-11-03 | depth | 根对象 `bomb` 深嵌套 skip | `BS_CONFIG_PARSE_ERR_SCHEMA` | `test_depth_*` + `skip_json_value` |
| SEC-11-04 | 重复键 | metadata 双 `amount` | `SCHEMA` | `test_duplicate_metadata_key` |
| SEC-11-05 | 类型混淆 | `"tax_rate": 13` | `SCHEMA` | `test_type_confusion_*` |
| SEC-11-06 | 注入串 | `"; DROP TABLE --"` | 解析成功、原样存储 | `test_injection_string_*` |
| SEC-11-07 | 截断 | 未闭合字符串 | `PARSE`/`LEX` + 行列 | `test_truncated_json_*` |
| SEC-11-08 | Report | 默认 gate parse 失败 | detail 含 `parse error at line` | `ReloadGateDefaultTest` |

**不在 parser 测例内**：`13%`、三位小数等 → `tools/normalize/examples/money_normalize.py` + CI。

**标签**：`ctest -C Release -L day11` → `bs_test_config_parse_security`。
