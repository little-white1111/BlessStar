# DAY15: Watch + Performance Baseline

本文件记录第15天方案 B（layered metrics + watch bus）的最小工程落地口径。

## 事件模型（WATCH-XV-1/2）

- 事件模型：`epoch + uri + stage + result`
- `stage` 覆盖：
  - `CAS`
  - `WAL_FSYNC`
  - `CANONICAL_WRITE`
  - `MANIFEST_FLIP`
  - `WAL_COMMIT`
  - `RECOVER_CONSERVATIVE`
- `result`：`OK` / `FAIL`

## 事件总线（WATCH-XV-3/4）

- 提供 in-process publish/subscribe：`bs_adapter_attach_persist_watch_subscribe` / `bs_adapter_attach_persist_watch_publish`
- 发布失败不阻断主流程：调用方统一忽略 `publish` 返回值，仅用于观测
- 统一失败观测口径（P0 修复）：
  - `publish_callback_error_count`
  - `callback_error_by_stage[stage]`
  - `last_callback_error_epoch` / `last_callback_error_stage`

## 发布点（WATCH-XV-5）

`attach_store.cpp` 中在关键事务阶段发事件：

1. CAS 校验（成功/冲突失败）
2. WAL append/fsync
3. canonical staging write
4. manifest flip
5. WAL committed

`attach_wal.c` 在恢复遇到 WAL corruption 时发 `RECOVER_CONSERVATIVE`。

## 指标与审计（WATCH-XV-6）

- `bs_adapter_attach_persist_watch_metrics_on_event`：
  - 统计总事件数
  - 分 stage 计数
  - 失败计数
  - 基于 `(epoch, uri_hash, stage)` 做幂等去重
  - 去重策略固定容量：`BS_ATTACH_WATCH_DEDUPE_SLOTS=1024`（P1 固化）
  - 暴露 `dropped_duplicates` 与 `dedupe_capacity`
- `bs_adapter_attach_persist_watch_audit_on_event`：
  - 统计 `RECOVER_CONSERVATIVE` 次数
  - 统计失败事件数量

## 测试覆盖

- `bs_test_attach_watch`（`-L day15`）验证：
  - 发布失败不阻断 `batch_commit`
  - 关键阶段事件被观测到
  - WAL 损坏恢复时审计计数递增

## 基线压测（T3）

执行文件：`build_ci_test/Release/bs_benchmark_attach_watch.exe`

标准化执行模板（P1 修复）：

- `powershell -ExecutionPolicy Bypass -File tools/scripts/perf/run_day15_watch_baseline.ps1`
- 产物默认输出：`docs/day15_watch_baseline_latest.txt`

输出（2026-05-28）：

- `64` 并发：`p95=200 ns`，`p99=400 ns`，`RSS delta=0.63 MB`
- `256` 并发：`p95=500 ns`，`p99=800 ns`，`RSS delta=0.88 MB`
- `500` 并发：`p95=200 ns`，`p99=400 ns`，`RSS delta=1.66 MB`
- summary：`total_events=134313`，`fail_count=0`，`conservative=0`，`rss_total_mb=0.96`

说明：

- 当前 benchmark 聚焦 watch publish 路径开销，不包含磁盘 IO（WAL/manifest）延迟。
- 结果用于第15天性能口径 baseline，后续可叠加端到端 attach 事务链路压测。

## 测试并发隔离规范（P0 修复）

- 所有 day15 相关测试必须使用唯一临时目录，禁止固定目录名。
- `AttachWatchTest` 已改为 `bs_day15_watch_<time_ns>_<random>` 目录模式，避免并行 `ctest` 互删冲突。

## 二期范围（P2）

- 端到端事务链路（`WAL→canonical→manifest`）P95/P99 基线
- watch 总线背压/限流/采样策略
- 跨进程事件总线与持久化重放

