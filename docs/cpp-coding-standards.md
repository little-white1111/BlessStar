# C++ 编码规范（初稿）

适用范围：本仓库 `kernel/` 与 `adapter/` 代码（第1天：骨架阶段）。

## 基本原则

- **可读性优先**：代码审阅以可读性与可维护性为第一标准。
- **最小暴露面**：头文件只暴露稳定 API；实现细节放在 `.cpp`。
- **无横向共享状态**：遵循“单线流水线”与模块边界约束，避免跨模块共享全局可变状态。

## 语言与工具链

- **C++ 标准**：C++17
- **编译器告警**：保持告警干净（第1天不强制 `-Werror`，后续可升级）

## 命名约定

- **命名空间**：使用 `ps::kernel::<module>`、`ps::adapter::<...>` 组织域边界
- **类型**：`PascalCase`（如 `PipelineResult`）
- **函数/变量**：`snake_case`（如 `read_file_or_empty`）
- **常量**：`kPascalCase` 或 `SCREAMING_SNAKE_CASE`（后续统一）

## 头文件与 include

- 头文件使用 `#pragma once`
- include 顺序建议：
  1) 本模块头文件
  2) 标准库
  3) 其他模块头文件
- **禁止越界 include**：下游模块不得 include 上游以外模块的私有头；仅通过公开 `include/` 目录访问。

## 错误处理

- 可预期失败优先返回错误对象/状态（如 `ok=false` + message）
- 不在库代码中直接 `std::exit`

## 格式化（占位）

第1天暂不引入 clang-format 文件，后续如需统一格式将补充 `.clang-format` 与 CI 格式检查门禁。

