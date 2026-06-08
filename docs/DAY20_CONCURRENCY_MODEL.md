# Day 20 — Attach 并发模型（方案 B″）

## 裁定摘要

- **方案**：AttachContext 会话级 `shared_mutex` + 写窗口（热更阻塞**新读**，已有读可完成）。
- **不变量**：`XX-CONC-1`～`8`（弱一致 + 单调 `revision`；staging 对读不可见；锁序；销毁序；listener 禁写等）。

## 锁与 API

| 组件 | 机制 |
|------|------|
| `attach_session` | `try_read_lock` / 写窗口 `begin_write_window` |
| `reload_batch_run` | 全程写窗口包裹 CM `sync_path` |
| `attach_watch.c` | 全局表 `mutex` |
| `g_active_ctx` | `std::mutex` + **AttachActiveGuard**（debug 无 guard 则 assert） |
| `attach_notify_queue` | ordered worker；`destroy` 前 **flush** |
| `emit_transition` | 两阶段：StateBus 提交 → EventBus drain → **phase2 watch**（可入队） |

## 读路径

- **meta**：`bs_adapter_attach_config_get_snapshot_meta`
- **全量拷贝**：`get_snapshot_copy`（≤ `BS_JSON_MAX_INPUT_BYTES`）
- **分块**：`open_snapshot_read` + `read_snapshot_chunk`（校验 `revision`）
- **强一致（可选）**：`wait_notify` / `read_since_meta`（超时 → `NOTIFY_TIMEOUT`）

## revision 对齐

- REC-G-03 后，产品 reload / recover 路径在每次成功 `config_sync` 后执行 post-sync：`path_revision` 直接写为 manifest revision，不再使用本地 `bump++` 作为产品真相。
- 读 API 会在存在 ctx-owned persist store 时刷新 manifest 并比较 `manifest_rev` 与 `path_revision`；不一致返回 `BS_ATTACH_ERR_REVISION_STALE`，不触发 lazy reload。
- 测试仍可直接调用 `config_sync` 而不 commit；该路径不代表产品写配置。

## Notify 弱一致与 flush

- Notify 仍是弱一致观察通道，真相以 StateBus + revision 为准；允许 listener 业务副作用滞后，但禁止 revision 回退。
- 最外层 `end_write_window` 负责完成边界：先 `drain_deferred_events`，再 `notify_queue_flush`；`reload_batch_run` 成功返回前必须清空写窗口内积压的 EventBus 事件和 phase-2 watch 队列。
- `attach_watch` 属于 persist 侧单次 publish 同步完成，不纳入 ConfigManager 写窗口 flush 的统一完成点。
- REC-G-03 不新增 notify QPS limiter，也不把 listener 业务副作用完成作为 MVP 闭合条件。

## 废弃 API（T20.3）

- `bs_adapter_attach_config_get_state` / `get_snapshot`（裸指针）仅保留给 **BS_TESTING** 遗留测例；新代码使用 meta + chunk。

## 错误码（attach 域）

见 `adapter/include/bs/adapter/attach_errors.h`：`REENTRANT` / `REVISION_CHANGED` / `TOO_LARGE` / `READ_BLOCKED` 等。

## 测试

```bash
ctest --test-dir build_ci_test -C Release -L day20 --output-on-failure
```

- **T20-0**：`bs_test_attach_watch_benchmark`（多线程 publish，纳入 `day20` 标签）
- **T20-2**：`AttachConcurrencyTest` 含 `PENDING_WRITE` → `READ_BLOCKED` 与 `wait_notify` 超时

Linux TSan（CI `tsan` job）：同上标签。

## 与第 21 天关系（KernelPool / A′′′）

- **XX-CONC-6**（裸调 `bs_kernel_execute` 竞态）由第 21 天 **KernelPool + A′′ executor** 闭合；详见 `docs/KERNEL_POOL_EXECUTOR_MODEL.md`。
- 第 20 天 **写窗口 / 单写者 persist** 不变；第 21 天仅在 **Phase B** 并行 exec，**Phase C** 仍串行 commit + CM sync。
- Linux TSan CI 除 `day20` 外增跑 **`kernel_pool`** 标签（`KernelPoolTest`、PER_BATCH 并行集成测）。

## 与第 19 天关系

- **day19** 长稳 profile 仍为**单写者多读者**文档化假设；并发压测使用 **day20** 标签，不混入 day19 smoke 门槛。
