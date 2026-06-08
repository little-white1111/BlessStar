# 扩大审查：`adapter/` + `kernel/runtime/`

**范围**：`adapter/**`（含 `parser/`、`persistence/`、`orchestration/`、`src/`）与 `kernel/runtime/**`（不含 `kernel/runtime/test` 风格细项）。

**工具**：`tools/scripts/audit_scope_adapter_runtime.py` + 人工扫 src + 门禁脚本。

---

## 自动化结论（public `include/*.h`）

| 检查项 | adapter + runtime | 说明 |
|--------|-------------------|------|
| C-ST-7 缺失 | **0** | 与 `check_public_header_contract_block.py` 一致 |
| C-ST-1 前缀 | **0** | 与 `check_public_api_prefix.py` 一致 |
| LEGACY-NOT-BUILT | **0** | parser 遗留头已移除（**17.22** / **XVII-DOC-1**） |

---

## 不一致点清单

### P1（建议修 / 已修）

| ID | 维度 | 位置 | 描述 | 状态 |
|----|------|------|------|------|
| **E-P1-01** | 注释 | `kernel/runtime/src/kernel.c` | 误导性「Assume pipeline destroy」注释，与 AttachContext 所有权不符 | ✅ 17.19 |
| **E-P1-02** | 注释 | `kernel/runtime/include/.../Kernel.h` | C-ST-7 未说明 pipeline **caller-owned** 与 `"default"` 名 | ✅ 17.19 |

### P2（风格 / 可修）

| ID | 维度 | 位置 | 描述 | 状态 |
|----|------|------|------|------|
| **E-P2-01** | 冗余 | `run_ir_execute_or_reject` | `gated_parse_active` 且无 `instructions` 时重复赋 `exec_rc=-1` | ✅ 17.19 |
| **E-P2-02** | 命名 | 测例 `mock_gate_pass` | 返回 `0` 而非 `BS_RELOAD_GATE_OK` | ✅ 17.19 |
| **E-P2-03** | 格式 | `attach_uri_path.h` | `#endif` 无 guard 注释 | ✅ 17.19 |
| **E-P2-04** | 命名 | attach 域 | C-ST-14 统一 `bs_adapter_attach_*` | ✅ 17.20 | `BLESSSTAR_NAMING_CONTRACT.md` | ✅ |

### P3（已知边界，不要求本轮清零）

| ID | 维度 | 位置 | 描述 |
|----|------|------|------|
| **E-P3-01** | 命名 | 类型 | `ReloadBatchController` vs `BsReloadGateDetail` |
| **E-P3-02** | 行为 | `default_path_gate` 回调 | 单独作 gate 时 parse 后 discard（非 batch 缓存路径） |
| **E-P3-03** | 注释 | 实现 `.c/.cpp` | 门禁只要求 **public 头** C-ST-7；实现文件无统一 file 头（非缺陷） |

---

## 第五轮（attach 热路径）+ 本轮（全 adapter/runtime public）

- attach/reload 第四轮清单：**已闭合**（见 `DAY17_INCONSISTENCY_CHECKLIST.md`）
- 扩大范围后 **无新增 P0**；**E-P1/E-P2 可修项** 见上表 ✅

**结论**：在「public 头 + 不变量 + 生产热路径」标准下，**无未闭合的 P0/P1**；**P3** 为类型命名风格等边界项。
