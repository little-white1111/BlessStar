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
| `g_active_ctx` | `std::mutex` 保护 set/get |
| `emit_transition` | 两阶段：StateBus 提交 → EventBus/Watch 通知 |

## 读路径

- **meta**：`bs_adapter_attach_config_get_snapshot_meta`
- **全量拷贝**：`get_snapshot_copy`（≤ `BS_JSON_MAX_INPUT_BYTES`）
- **分块**：`open_snapshot_read` + `read_snapshot_chunk`（校验 `revision`）

## 错误码（attach 域）

见 `adapter/include/bs/adapter/attach_errors.h`：`REENTRANT` / `REVISION_CHANGED` / `TOO_LARGE` / `READ_BLOCKED` 等。

## 测试

```bash
ctest --test-dir build_ci_test -C Release -L day20 --output-on-failure
```

Linux TSan（CI `tsan` job）：同上标签。

## 与第 19 天关系

- **day19** 长稳 profile 仍为**单写者多读者**文档化假设；并发压测使用 **day20** 标签，不混入 day19 smoke 门槛。
