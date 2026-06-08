# DAY16: API Contract Samples

本文对应 `IMPL-16-03`，给出 `attach_store`、`attach_watch`、`io` 的对接契约样例。

## 1) attach_store 契约样例

- `api`: `bs_adapter_attach_persist_store_batch_commit`
- `call_order`: `batch_begin -> batch_stage* -> batch_commit`
- `must`: 保持事务阶段顺序 `CAS -> WAL_FSYNC -> CANONICAL_WRITE -> MANIFEST_FLIP -> WAL_COMMIT`
- `must_not`: 任何 watch 回调不得反向改写提交结果
- `return`: `BS_ATTACH_OK` / `BS_ATTACH_ERR_*`

## 2) attach_watch 契约样例

- `api`: `bs_adapter_attach_persist_watch_publish`
- `event_model`: `epoch + uri + stage + result`
- `must`: 发布失败不阻断主流程
- `must`: 审计字段必须可追溯（epoch/stage）
- `idempotency`: `(epoch, uri_hash, stage)` 去重

## 3) io 契约样例

- `api`: `bs_io_facade_read`
- `caller`: adapter/orchestration
- `callee`: resolved provider ops
- `must`: respect `BS_IO_MAX_READ_BYTES` and timeout
- `return`: `BS_IO_OK` / `BS_IO_ERR_*`
- `must_not`: 在 io 层植入业务语义解析
