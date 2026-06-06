# Day 21 — KernelPool + A′′ Executor（A′′′）

## 裁定摘要

- **方案**：AttachContext 级 **KernelPool**（steady **3** / max **10**）+ 每 slot **A′′ ordered worker** + **IrSnapshotStore（4A pin）**。
- **编排**：PER_BATCH **1A** — Phase A gate+publish → Phase B 并行 `pool_submit` → Phase C 单次 commit + CM sync。
- **契约**：**C-KERNEL-POOL-1**（`docs/contracts/kernel/kernel_pool.v1.json`）；门禁 **GATE-KERNEL-POOL-CONFIG**。

## 不变量

| ID | 语义 |
|----|------|
| **KERNEL-POOL-7** | PER_BATCH 批内 exec 可并行（每 path 占一 slot） |
| **ORCH-POOL-1** | reload 主链 gate → exec → persist（**G-10**） |
| **IR-SNAPSHOT-1/2/4A** | gate 后 publish；exec **不得**再 parse；submit 前 pin / 完成后 unpin |
| **INC-VIII-2** | `kernel_pool.c` **不得** include `ConfigManager.h` |
| **3A** | exec 失败 **不** quarantine slot；slot 回 IDLE 复用 |
| **2A** | 满池时 FIFO 无界 wait（背压，非 fail-fast） |

## 组件

| 组件 | 路径 | 职责 |
|------|------|------|
| A′′ Executor | `kernel/runtime/src/kernel.c` | PipelineRef、ordered worker、INLINE 深度、`P1-A` stop drain |
| KernelPool | `kernel/runtime/src/kernel_pool.c` | steady/dynamic 调度、wait_queue、warmup/drain |
| IrSnapshotStore | `adapter/src/attach_ir_snapshot.cpp` | publish / pin / unpin |
| reload 编排 | `adapter/orchestration/reload_batch_controller.cpp` | PER_PATH / PER_BATCH Phase A/B/C |
| exec 门面 | `adapter/src/attach_execute.cpp` | 委托 `bs_kernel_pool_submit` |
| warmup | `adapter/src/registry_bootstrap.cpp` | freeze 成功后 `kernel_pool_warmup` |

## reload exec 门禁

- **仅** `bs_adapter_attach_ctx_is_kernel_pool_warmed(ctx)==1` 的全量 AttachContext（freeze 后）走 IrSnapshot + pool exec。
- 无 pool 的 ephemeral / 单测 ctrl **仍**走 legacy gate_fn（mock gate 不强制 parse）。

## 失败语义

- 任一 path exec 失败 → batch **abort**（**XVII-CM-3**）；**不** persist / **不** CM sync。
- **无** execute 自动重试；失败上报 abort，slot 回 IDLE。

## 停止语义（P1-A / P2-A · 用户闭合裁定）

| 场景 | API / 行为 |
|------|------------|
| 单 Kernel slot | `bs_kernel_stop` → **STOPPING** → 拒新 job → **P1-A** 等在跑 job 完成 → flush/join → 唤醒 waiter（`BS_KERNEL_ERR_STOPPED`） |
| 整个 Pool | `bs_kernel_pool_destroy` → **POOL_DRAINING** → reject submit → 全 slot **P1-A** drain → 释放资源 |
| **execute 超时** | **P2-A**：MVP **不提供**；第21天 **不**实现超时 API |
| **协作式 cancel** | **P1-A**：**不做** |

## Sanitizer 门禁（TSan）

- **MVP 硬门禁**：**Linux** GitHub Actions `ci.yml` → job **`tsan (Linux Clang day20)`**，`ctest -L "day20|kernel_pool"`。
- **Windows TSan**：**非** MVP 门禁；不以 Windows 本地 TSan 作为闭合条件。
- **取证**：PR 合并前/后 Actions run URL 写入 `项目修改记录.md` **21.7** 与架构 § **IMPL-21-CI-01**。

## 与第 19 天 profile 关系（**XIX-MEM-8** / **R21-04**）

- **day19** 默认 harness profile 仍为**单写者多读者**，**不**启用 pool 并行 exec。
- pool 并行与 TSan 覆盖使用 **`kernel_pool`** / **`kernel_conc`** 标签，与 day19 smoke **分 profile** 统计。

## 测试

```bash
ctest --test-dir build_ci_test -C Release -L kernel_pool --output-on-failure
ctest --test-dir build_ci_test -C Release -R bs_test_reload_per_batch_parallel_exec --output-on-failure
```

- **T-POOL-16**：`KernelExecutorTest` + `KernelPoolTest`
- **T-POOL-17**：`ReloadPerBatchParallelExecTest`（3 path PER_BATCH 并行 exec）

Linux TSan（CI `tsan` job）：`ctest -L kernel_pool` + `ctest -R bs_test_attach_concurrency`（**不含** `attach_watch_benchmark`；benchmark 在 TSan 下过慢）。

## 交叉引用

- 第 20 天 Attach 并发（写窗口、notify）：`docs/DAY20_CONCURRENCY_MODEL.md`
- 契约实例：`docs/contracts/kernel/kernel_pool.v1.json`
