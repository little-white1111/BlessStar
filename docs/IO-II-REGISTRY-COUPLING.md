# IO-II 与 Registry 耦合说明（R8-05 / IMPL-08-05）

| 字段 | 值 |
|------|-----|
| **状态** | ✅ 已采纳（第8天 · REV-VIII-10） |
| **日期** | 2026-05-18 |
| **关联** | 第6天 IO-II-1～5 · `ADR-BS-IO-001` · R8-05 |

## 1. 裁定摘要

**R8-05 方案 A**：MVP **不拆分** `IoRegistryPort` 等薄抽象；**承认** `bs_kernel_io` 在 CMake 与运行时上依赖 `bs_kernel_registry`。通过本文档固定 **调用顺序** 与 **测试约定**，避免「无 Registry 的 IoFacade 单测」与生产路径分叉。

## 2. IO-II 不变量（复述 · 已确认）

| ID | 条文 | 工程含义 |
|----|------|----------|
| IO-II-1 | 内核 IO 不解析业务格式 | `bs_io_facade_read` 返回字节；ini/xml 在 adapter/parser |
| IO-II-2 | 单向依赖 | `kernel/io` **不得** `#include` `bs/adapter/*` |
| IO-II-3 | Registry 边界 | Provider 绑定 `/adapter/io/...`；**不得**在 Provider 内缓存 IR/需求清单 |
| IO-II-4 | 安全与审计 | `max_read`、失败码、`error_message`；adapter 挂 Report |
| IO-II-5 | freeze 对齐 | Provider **注册/bind** 须在 `bs_registry_facade_freeze()` **之前** |

**热更 / watch / 批控交界**：见 `架构方案选择记录.md` · **ADR-BS-IO-001**（`freeze` 后允许 read、禁止结构性写等）。

## 3. 运行时耦合（R8-05 核心）

### 3.1 对象关系

```text
RegistryFacade*  ──owns──▶  PathRegistry + Hub + 相位 + 域表
       ▲
       │ bs_io_facade_create(registry)
       │
   IoFacade*  ──read 时──▶  bs_registry_facade_resolve(..., "/adapter/io/...")
```

- `IoFacade` **不拥有** `RegistryFacade`；生命周期由 attach/bootstrap 或测试 fixture 管理。  
- **每次 `bs_io_facade_read`**（及内部 resolve 路径）假定 registry 已处于可 resolve 状态：对应 Provider 已 **bind**，且通常已 **freeze**（生产 attach 链）。

### 3.2 调用顺序（attach 主链）

与 `bs_adapter_registry_bootstrap_*` 一致：

```text
1. bs_registry_facade_create()
2. bs_adapter_registry_bootstrap_begin[_ctx]     → P1，内置 /kernel，log 域
3. bs_adapter_registry_bootstrap_register_standard_io[_ctx]  → P2，/adapter/io/*
4. bs_adapter_registry_bootstrap_freeze[_ctx]    → FROZEN
5. bs_io_facade_create(facade)
6. bs_io_facade_read(facade, uri, &result)       → resolve + Provider.read
```

**禁止**：在步骤 4 之前对依赖已注册 Provider 的 URI 做生产级 read（可能 `BS_IO_ERR_NO_PROVIDER` / `BS_REGISTRY_ERR_*`）。

### 3.3 CMake 依赖（事实）

```text
bs_kernel_io  PUBLIC  →  bs_kernel_registry  →  bs_kernel_common
```

新增 IO 能力若需 resolve，**默认**继续链 `bs_kernel_registry`；若未来拆 `IoRegistryPort`，须单独立项 ADR（**非**本期 MVP）。

## 4. 测试约定（「接受完整 registry」）

| 场景 | 做法 | 反例 |
|------|------|------|
| IoFacade 单测 | `bs_registry_facade_create` + `advance_phase` + 注册/ bind provider + `freeze` + `bs_io_facade_create` | 仅 mock `IoProvider` 而不走 PathRegistry |
| attach 集成 | `bs_adapter_registry_bootstrap_*` 或 `AttachPipelineRegistryTest` 同级 setup | 在测试中手写 `domain_id==1` 而不 `out_domain_id` |
| 内核 purity | `kernel/**/test` **不得** `#include` `bs/adapter/`（INC-VIII-1） | 在 `kernel/io/test` 里 include `io_providers.h` |

**参考用例**：

- `adapter/test/IoAttachPipelineTest.cpp` — bootstrap → freeze → read  
- `kernel/io/test/IoFacadeTest.cpp` — 最小 registry + phase + bind  
- `adapter/test/AttachPipelineRegistryTest.cpp` — P2 插件 + freeze + resolve  

## 5. 与第8天其它裁定的关系

| 裁定 | 关系 |
|------|------|
| R8-02 AttachContext | log/read 守卫经 ctx；**不**改变 IO 须经 registry resolve |
| R8-06 report 双库 | reload 链 `bs_kernel_report_core`；IO read 仍经 facade |
| R8-12 链接扇出 | 见 `docs/PHASE1_ARCHITECTURE_REVIEW.md` §3 |
| ADR-BS-ABI-001 | IoFacade/RegistryFacade 均为 C ABI 不透明句柄 |

## 6. 非目标（本期）

- 不实现 `IoRegistryPort` 注入抽象。  
- 不要求 IoFacade 在无 registry 场景下行为完整（可返回错误，但**不**作为支持路径文档化）。  
- 不替代 `ADR-BS-IO-001` 中 state/watch/GC 条文。

## 7. 相关文档

- `docs/adr/ADR-BS-ABI-001.md`  
- `docs/PHASE1_ARCHITECTURE_REVIEW.md`  
- `架构方案选择记录.md` · 第6天 IO-II · 第8天 R8-05
