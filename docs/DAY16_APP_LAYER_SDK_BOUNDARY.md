# DAY16: App Layer (SDK) Boundary Draft

本文对应 `IMPL-16-01`，定义新增 `App Layer (Business SDK/Library)` 的职责边界与建议目录。

## 1. 分层职责

- `Kernel`：仅消费通用 IR/指令，不感知业务字段与业务流程。
- `Adapter/Runtime`：负责 `io -> parser -> gate -> attach -> watch` 通用链路，不承载业务语义决策。
- `App Layer (SDK)`：承接业务配置模型、业务校验、业务语义到通用 IR 映射。

## 2. 硬边界

- `App` 不直接写 `kernel` 内部状态，只通过已定义对接契约进入 `adapter`。
- `Adapter` 禁止出现业务专有字段规则（如具体财务科目语义）。
- `Kernel` 禁止依赖业务 SDK 目录与业务 schema 文件。

## 3. 建议目录（草案）

```text
app/
  sdk/
    include/bs/app/sdk/
      app_contract.h
      app_config_model.h
      app_ir_mapper.h
    src/
      app_config_model.cpp
      app_ir_mapper.cpp
      app_contract.cpp
    examples/
      scenario_expense_reimburse.cpp
      scenario_gl_mapping.cpp
```

## 4. 迁移原则

- 先文档化与样例化，再逐步把业务语义从 `adapter` 外移到 `app/sdk`。
- 不改变第14/15天已确认事务时序与持久化边界。
