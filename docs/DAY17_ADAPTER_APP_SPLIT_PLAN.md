# DAY17: Adapter -> App Split Plan

## 1. 判定铁则

- 换行业/换业务就会变的规则：迁到 `app/sdk`
- 任何业务都一致的机制：留在 `adapter`

## 2. 迁移矩阵（首批）

| 模块/职责 | 当前位置 | 目标位置 | 结论 |
|----------|----------|----------|------|
| 业务场景模型（费用报销/总账映射） | adapter 未显式分层 | `app/sdk` | 迁移 |
| 场景级可变校验策略 | adapter gate 周边 | `app/sdk` | 迁移 |
| IO 抽象与 provider | `adapter/io` | `adapter/io` | 保留 |
| parse/gate 通用执行框架 | `adapter/orchestration` | `adapter/orchestration` | 保留 |
| attach WAL/manifest 事务语义 | `adapter/persistence` | `adapter/persistence` | 保留 |
| watch 最小事件模型 | `adapter/persistence` | `adapter/persistence` | 保留 |

## 3. 分阶段执行

- P0：补充 `app/sdk` 场景模型与基础校验接口（不改变主链）
- P1：在 `ReloadBatchController` 增加 app 预检查 hook（可选）
- P2：将业务可变规则从 adapter 周边逻辑逐步迁移到 app 侧实现
