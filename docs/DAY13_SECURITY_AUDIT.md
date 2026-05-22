# Day13 Security Audit Report

> **架构**：方案 C + 解法 3（AUD-IX）  
> **Release 基线**：Windows Release **65/65**（含 `bs_test_config_parse_security_audit`）  
> **Sanitizer**：Linux Clang ASan+UBSan CI job（`ci.yml` · `sanitizer` · blocking）

## 双门禁

| 门禁 | 配置 | 状态 |
|------|------|------|
| 发布 | Windows/Linux **Release** 全量 ctest | **65/65**（Windows Release 已验证） |
| 安全 | **ubuntu-latest** · Clang · `-fsanitize=address,undefined` | CI blocking（待 Actions 绿） |

## 风险分级与闭合映射

| ID | 级别 | 主题 | IMPL | 测例 / 证据 |
|----|------|------|------|-------------|
| AUD-P-01 | P0 | `decode_json_string` 边界 | IMPL-13-01/02 | `json_parser.c` · `o < BS_JSON_MAX_STRING_BYTES` |
| AUD-P-07 | P0 | instructions 无上限 | IMPL-13-03 | `-L day13` · 2048/2049 |
| AUD-P-15 | P0 | parse 失败泄漏 | IMPL-13-09 | Sanitizer + `free_instruction_list` |
| AUD-P-08 | P1 | manual 数组 | IMPL-13-04 | `-L day13` · 256/257 |
| AUD-A-01 | P1 | manifest 行长 | IMPL-13-05 | `-L day13` · `BS_ATTACH_MAX_MANIFEST_LINE` |
| AUD-P-09 | P1 | session 字节加法 | IMPL-13-06 | `reload_batch_controller.cpp` 饱和检查 |
| AUD-P-02 | P1 | `strcpy` → `memcpy` | IMPL-13-07 | `keys_seen_add` |
| AUD-P-05/06 | P2 | snprintf | IMPL-13-08/10 | `bs_safe_*` · gate/audit/log/report（BlessStar 主树已闭合，仅 `bs_safe_format.c` 包装 `vsnprintf`） |
| AUD-P-12 | P2 | 1MiB 输入 | — | day10 回归 |
| AUD-FUZZ-01 | P2 | fuzz | IMPL-13-17 | `DAY13_FUZZ_DESIGN.md`（无代码） |

## 资源常量（AUD-IX-4）

| 常量 | 值 |
|------|-----|
| `BS_JSON_MAX_STRING_BYTES` | 16384 |
| `BS_JSON_MAX_STRING_LEN` | 4096（码点，与 day10 一致） |
| `BS_JSON_MAX_INSTRUCTIONS` | 2048 |
| `BS_JSON_MAX_MANUAL_ITEMS` | 256 |
| `BS_ATTACH_MAX_MANIFEST_LINE` | 8192 |

## 刻意未改语义

- **SEC-IX** / **RES-IX** / **PARSE-IX-4** 行为不变；仅实现层加固与工程质量门禁。
