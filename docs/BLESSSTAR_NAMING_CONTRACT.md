# BlessStar 分层命名规范（C-ST-14）

**状态**：active · **版本**：v1 · **维护**：子任务 B  
**机器门禁**：`tools/scripts/gates/check_layered_api_prefix.py`（GATE-STYLE-LAYERED）

## 1. 总则

| 项 | 规则 |
|----|------|
| C API 前缀 | 必须以 `bs_` 开头（C-ST-1） |
| 路径对齐 | `include/bs/<层>/<模块>/` 与 CMake `bs_<层>_<模块>` 一致 |
| 新公共头文件 | snake_case；PascalCase 仅维护例外表（XVII-NAMING-1） |
| 公共头契约 | C-ST-7（Thread safety / Error semantics / Platform notes） |
| LEGACY | 含 `LEGACY-NOT-BUILT` 的头不进门禁、不链接 |

## 2. Kernel 层（`kernel/`）

```text
bs_<模块>_<对象>_<动词>
```

| 模块 | 前缀示例 |
|------|----------|
| `common` | `bs_status_*`、`bs_log_*`、`bs_reentrancy_*` |
| `io` | `bs_io_*` |
| `ir` | `bs_ir_*` |
| `state` | `bs_config_manager_*`、`bs_event_bus_*` |
| `registry` | `bs_registry_facade_*` |
| `pipeline` | `bs_pipeline_*`、`bs_stage_*` |
| `runtime` | `bs_kernel_*`、`bs_context_*`（runtime Context） |
| `report` | `bs_report_*` |

**禁止**：在 kernel public 头中使用 `bs_adapter_*`。

## 3. Adapter 层（`adapter/`）

```text
bs_adapter_<模块>_<...>
```

| 模块目录 | 前缀 |
|----------|------|
| `io/` | `bs_adapter_io_*` |
| `log/` | `bs_adapter_log_*` |
| `registry/` | `bs_adapter_registry_*` |
| `parser/`（主链） | `bs_adapter_parser_*` |
| `requirement/` | `bs_adapter_requirement_*` |
| `plugin/` | `bs_adapter_plugin_*` |

### 3.1 Attach 子域（统一前缀）

```text
bs_adapter_attach_<子域>_*
```

| 子域 | 前缀 | 职责 |
|------|------|------|
| `ctx` | `bs_adapter_attach_ctx_*` | AttachContext 会话 |
| `config` | `bs_adapter_attach_config_*` | ConfigManager 桥接 |
| `exec` | `bs_adapter_attach_exec_*` | Kernel IR 执行 |
| `reload` | `bs_adapter_attach_reload_*` | Batch 编排、default gate |
| `persist` | `bs_adapter_attach_persist_*` | store / wal / watch / report |

**禁止的新 public C 符号**（C-ST-14）：`bs_attach_context_*`、`bs_reload_batch_*`、`bs_attach_store_*`、`bs_config_parse_*`、`bs_adapter_attach_execute_*` 等迁移前前缀。

## 4. App 层（`app/sdk/`）

| 项 | 规则 |
|----|------|
| C++ | 命名空间 `bs::app`；类型 PascalCase |
| 可选 C ABI | `bs_app_sdk_*` |

## 5. 类型与枚举

- 枚举/宏：`BS_ATTACH_*`、`BS_ORCH_*` 等可保留（非函数）。
- 结构体：渐进采用 `BsAdapterAttach*`；历史名 `AttachContext`、`ReloadBatchController` 允许保留。

## 6. 迁移对照（adapter attach 主链）

| 迁移前 | 迁移后 |
|--------|--------|
| `bs_attach_context_create` | `bs_adapter_attach_ctx_create` |
| `bs_reload_batch_run` | `bs_adapter_attach_reload_batch_run` |
| `bs_attach_store_open` | `bs_adapter_attach_persist_store_open` |
| `bs_config_parse_bytes` | `bs_adapter_parser_parse_bytes` |
| `bs_adapter_attach_execute_gated_ir` | `bs_adapter_attach_exec_gated_ir` |

完整历史见 `项目修改记录.md` 第 17.20 天。
