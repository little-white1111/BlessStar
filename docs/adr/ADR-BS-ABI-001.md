# ADR-BS-ABI-001 · 对外 C ABI、对内 C++17（R8-04 / IMPL-08-04）

| 字段 | 值 |
|------|-----|
| **状态** | ✅ 已采纳（第8天 · REV-VIII-9） |
| **日期** | 2026-05-18 |
| **关联** | `架构方案选择记录.md` · R8-04 · IMPL-08-04 |

## 背景

BlessStar 内核与 adapter 边界需同时满足：

- 与 C 调用方、插件 `register_fn`、跨模块测试的 **稳定 ABI**；
- 实现层可用 **C++17**（`std::string`、`std::vector`、`std::mutex` 等）以降低工程成本。

第8天研析指出 `registry_facade.cpp`、`path_registry.cpp` 等 **C 头 + C++ 实现** 混用；若边界不清，易出现异常穿出、ODR/布局不一致、测试与生产 ABI 分裂。

## 决策

1. **对外（跨库 / 跨进程 / 插件入口）**：仅暴露 **C ABI**  
   - 头文件使用 `extern "C"`（C++ 编译单元 include 时）。  
   - 导出符号以 `bs_*` / `Bs*` 类型为主；**禁止**在公共头中暴露 C++ 类、模板、`std::` 类型。  
   - 错误以 **`BsStatus`（`int`）** 与输出参数返回；**禁止** C++ 异常穿过 ABI 边界。

2. **对内（`.cpp` 实现单元）**：**C++17**  
   - `CMAKE_CXX_STANDARD 17`（见根 `CMakeLists.txt`）。  
   - 实现文件可使用 STL、RAII、`std::nothrow`；异常 **不得** 逃逸到 C 导出函数外——须在边界捕获并映射为 `BsStatus` 或 `BS_REGISTRY_ERR_*` / `BS_IO_ERR_*`。

3. **不透明句柄**  
   - `RegistryFacade*`、`IoFacade*`、`AttachContext*`、`PathRegistry*` 等对调用方为 **不透明指针**；布局仅定义在对应 `.cpp` 内。

4. **与错误模型分工（第7～8天）**  
   - **薄错误**：`BsStatus` + 域表（`bs_status_make` / `out_domain_id`）。  
   - **富错误**：批次 → `Report`；单点 → `error_message` / `bs_log`（经 `AttachContext` / `BsLogState`）。  
   - **已废弃**：`BsError`（IMPL-08-01）。

## 非目标（MVP）

- 不引入 `extern "C++"` 导出或 COM/WinRT 风格接口。  
- 不在本期统一「全项目禁止异常」——仅约束 **ABI 边界**；内核内部测试仍可按模块选用 C++ 风格（见 `docs/CODING_STYLE.md` §7 与本文差异说明）。  
- 不拆分 `registry_facade` 为纯 C 实现（成本过高；维持 C 头 + C++ 实现）。

## 证据锚点（现状树）

| 区域 | 路径 | 模式 |
|------|------|------|
| Registry C API | `kernel/registry/include/bs/kernel/registry/registry_facade.h` | `extern "C"` + `RegistryFacade*` |
| Registry 实现 | `kernel/registry/src/registry_facade.cpp` | C++17 + `std::vector` |
| IO Facade | `kernel/io/include/bs/kernel/io/io.h` | C ABI；`io_facade.cpp` 持有 `RegistryFacade*` |
| Attach | `adapter/include/bs/adapter/attach_context.h` | C API；`attach_context.cpp` 为 C++ |
| 插件入口（规划） | IMPL-08-17 · `entry.register_fn` | **必须** 遵守本 ADR |

## 实现检查清单（子任务 B / Code Review）

- [ ] 新增 `bs_*` 导出函数：无异常逃出；失败返回码明确。  
- [ ] 公共头：无 `std::`、`class` 成员暴露给调用方。  
- [ ] 插件/adapter 注册回调：签名仅用 C 类型与函数指针。  
- [ ] 测试可通过 C API 链接，无需 include adapter 实现头（内核测用 `bs_kernel_test_support`）。

## 后果

- **正面**：ABI 稳定、与第5～7天 Registry/IO/Status 叙事一致；便于 08-17 静态插件 `register_fn`。  
- **负面**：实现层仍依赖 C++，调试需区分「头文件承诺」与「实现细节」。  
- **二期**：若需 `format_fn` per-domain（R8-10 C 参考），须在 ADR 附录登记，不得隐式改 C 结构体布局。

## 相关文档

- `docs/IO-II-REGISTRY-COUPLING.md`（IO 与 Registry 耦合 · R8-05）  
- `docs/PHASE1_ARCHITECTURE_REVIEW.md` §3（adapter 链接扇出 · R8-12）  
- `docs/CODING_STYLE.md` §11（ABI 与 include 补充）
