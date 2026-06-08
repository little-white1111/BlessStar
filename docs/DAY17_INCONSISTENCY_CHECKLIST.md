# DAY17: 不一致点审查清单（第四轮闭环）

对应 **变更记录 17.17 / 17.18**；门禁与回归均已通过。

| # | 维度 | 位置/证据 | 不一致描述 | 关联契约 | 修复策略 | 状态 |
|---|------|-----------|------------|----------|----------|------|
| 1 | 命名 | attach 域三前缀 | C-ST-14 `bs_adapter_attach_*` 子域 | XVII-ATTACH / C-ST-14 | `docs/BLESSSTAR_NAMING_CONTRACT.md` | ✅ |
| 2 | 命名 | default gate 双 API | `use_default_gate` 与 `set_default_gate` 重复 | XVII-KERNEL-4 | 移除 `use_default_gate`，统一 `set_default_gate` | ✅ |
| 3 | 命名 | 手动 set_gate_fn | 未置 cache 导致双 parse | XVII-KERNEL-4 | `set_gate_fn` 识别 `default_path_gate` | ✅ |
| 4 | 注释 | `attach_runtime.h` | C-ST-7 与 log-ready 实现不符 | C-ST-7 | 重写 contract block | ✅ |
| 5 | 注释 | `attach_execute.h` | 未文档 parsed_ir / skip 语义 | XVII-KERNEL-4/8 | 补 C-ST-7 + 函数说明 | ✅ |
| 6 | 注释 | `reload_gate_default.h` | 未写单次 parse 共享 | XVII-KERNEL-4 | 补 Platform notes | ✅ |
| 7 | 注释 | `run_ir_execute_or_reject` | 缓存失效回退未说明 | XVII-KERNEL-4 | 块注释 + 禁止隐式二次 parse | ✅ |
| 8 | 注释 | 测例 gate reject | 无 `GATE_REJECTED` 断言 | ADAPTER-ORCH-GC-01 | `ReloadBatchControllerTest` | ✅ |
| 9 | 冗余 | gate 分支 | 双份 `release_gated_parse` | — | 合并到 if 前 | ✅ |
| 10 | 冗余 | `attach_execute.cpp` | 重复 kernel 前置检查 | — | `resolve_running_kernel` | ✅ |
| 11 | 文档 | `架构方案选择记录` | XVII-KERNEL-4 未写单次 parse | XVII-KERNEL-4 | 补「变更 17.17」 | ✅ |

**门禁（2026-05-29）**

- `check_public_api_prefix.py` ✅
- `check_public_header_contract_block.py` ✅
- `ctest -L day17 -E contract_gate_runner` ✅ 20/20
- reload/attach 相关回归 ✅

**保留为已知、非缺陷**

- `ReloadBatchController` vs `BsReloadGateDetail` 类型前缀风格（历史）

**LEGACY parser 头**：已于 **17.22** 删除（**XVII-DOC-1** 迁移并移除）。

**第五轮审查**：无新增 P0/P1/P2 项。
