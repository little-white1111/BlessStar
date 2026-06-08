# DAY16: API Guide Draft

本文对应 `IMPL-16-04`，提供 API 使用初稿与错误码速查。

## 1. 快速流程

1. `bs_io_facade_read` 读取配置字节。
2. parser + gate 完成结构与安全校验。
3. `bs_adapter_attach_persist_store_batch_*` 执行原子提交。
4. `bs_adapter_attach_persist_watch_publish` 输出观测事件。

## 2. 关键 API

- Persistence: `bs_adapter_attach_persist_store_open/close`, `batch_begin/stage/commit/abort`
- Watch: `bs_adapter_attach_persist_watch_subscribe/publish/unsubscribe`
- IO: `bs_io_facade_create/read/stat/destroy`

## 3. 错误码速查（摘要）

### Attach

- `BS_ATTACH_ERR_INVALID_ARG`: 参数非法
- `BS_ATTACH_ERR_CONFLICT`: revision 冲突
- `BS_ATTACH_ERR_IO`: IO 失败
- `BS_ATTACH_ERR_LIMIT`: 资源/上限约束触发

### IO

- `BS_IO_ERR_INVALID_URI`
- `BS_IO_ERR_READ_LIMIT`
- `BS_IO_ERR_TIMEOUT`
- `BS_IO_ERR_NO_PROVIDER`

## 4. 常见误用

- 在 `adapter` 中加入业务字段判定（应放 `App Layer SDK`）
- 未调用 `batch_begin` 即 `batch_stage`
- 将 watch 失败视为提交失败（违反契约）
