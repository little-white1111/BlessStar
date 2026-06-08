# DAY16: Contract Templates (4 Types)

本文对应 `IMPL-16-02`，提供契约模板：架构、功能、配置、对接；第17天新增 **风格契约**（见 `docs/DAY17_STYLE_CONTRACT.md`）。

**冲突优先级（第17天 C-IX-6′）**：架构 > 对接 > 风格 > 功能 > 配置。

## 架构契约模板

- `contract_id`:
- `name`:
- `scope`: kernel / adapter / app / cross-layer
- `rule`:
- `forbidden`:
- `priority`: high
- `verify`: lint/check/test id
- `owner`:

## 功能契约模板

- `contract_id`:
- `module`:
- `responsibility`:
- `non_responsibility`:
- `input`:
- `output`:
- `failure_semantics`:
- `verify`:

## 配置契约模板

- `contract_id`:
- `config_source`:
- `parse_rule`:
- `ir_mapping_rule`:
- `compatibility`:
- `fallback_policy`:
- `error_code_mapping`:
- `verify`:

## 风格契约模板（第17天 · C-ST-*）

- `contract_id`: `C-ST-n`
- `name`:
- `scope`: kernel / adapter / app / test / cmake / docs
- `rule`:
- `verify`: lint / doc-check / cmake 脚本 ID
- `priority`: style
- `owner`:
- `status`: draft / active

## 对接契约模板

- `contract_id`:
- `api`:
- `caller` / `callee`:
- `call_order`:
- `args_constraints`:
- `return_semantics`:
- `idempotency`:
- `timeout_retry`:
- `observable_fields`:
- `verify`:
