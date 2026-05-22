# BlessStar C++ Coding Style Guide

This document defines the coding style for BlessStar kernel development. It is based on the LLVM coding style with modifications for kernel-specific requirements.

## 2. Formatting

### 2.1 Indentation
- Use **4 spaces** for indentation (no tabs)
- Continuation lines should be indented by 4 additional spaces

### 2.2 Line Length
- Maximum line length: **100 characters**
- Break long lines at appropriate points

### 2.3 Brace Style
- Use **Allman style** (braces on their own line)
- Always use braces for control statements

```cpp
if (condition)
{
    doSomething();
}
```

### 2.4 Whitespace
- No trailing spaces
- One blank line between functions
- Two blank lines between classes/structs

## 3. Naming Conventions

### 3.1 General Rules
- **Classes/Structs**: PascalCase
- **Functions**: PascalCase
- **Variables**: camelCase
- **Constants**: UPPER_CASE_WITH_UNDERSCORES
- **Namespaces**: lowercase_with_underscores

### 3.2 Examples

```cpp
class InputIR { ... };
void ProcessInput(const IR& input);
int pipelineResult;
const int MAX_BUFFER_SIZE = 1024;
namespace ps::kernel::ir { ... }
```

## 4. Header Files

### 4.1 Include Guards
- Use `#pragma once` for include guards

### 4.2 Include Order
1. Header file for the current source file
2. Standard library headers (alphabetically)
3. Project headers (alphabetically)

## 5. Classes and Structs

### 5.1 Access Control
- Order: `public`, then `protected`, then `private`

### 5.2 Constructors
- Use member initialization lists

## 6. Functions

### 6.1 Parameters
- Keep parameter lists short
- Use const references for non-trivial types

## 7. Error Handling

- **ABI 边界**（`extern "C"` 导出、`bs_*` API）：**禁止** C++ 异常穿出；使用 `BsStatus` / `BS_*_ERR_*` 返回码。见 `docs/adr/ADR-BS-ABI-001.md`。
- **内核 / adapter 内部 `.cpp`**：可使用 C++17；若在 C 导出函数内调用可能抛异常的代码，须在边界捕获并映射为错误码。
- 历史文档曾写「Use exceptions for error handling」——**以 ADR-BS-ABI-001 为准**覆盖 ABI 层表述。

## 8. Comments
- Use Doxygen style for documentation comments
- Keep inline comments concise

## 9. Versioning
- Use semantic versioning for the kernel API
- Mark deprecated APIs with `[[deprecated]]` attribute

## 10. Formatting Tools
- Use clang-format for automatic formatting
- Use clang-tidy for static analysis

## 11. ABI 与 Include（第8天 · R8-04 / R8-13）

### 11.1 C ABI 边界

- 公共头仅 C 可链接类型；实现放在 `.cpp`。详见 **`docs/adr/ADR-BS-ABI-001.md`**。

### 11.2 Include 纪律

- **禁止**为省事 `#include` `ConfigManager.h`：若只需 `EventBus` / `StateBus`，include 对应子头（`EventBus.h` 等）。`ConfigManager.h` 已前向声明子组件（IMPL-08-21）。
- **`kernel/**/test`**：**禁止** `#include` `bs/adapter/*`；Log mock 使用 `bs_kernel_test_support`。
- CI：`python tools/purity/check_includes.py`（INC-VIII-1/2）。

### 11.3 IO 与 Registry

- `bs_io_facade_read` 前须完成 Provider 注册与（生产路径）`freeze`。详见 **`docs/IO-II-REGISTRY-COUPLING.md`**。

---

*Based on LLVM coding standards with BlessStar-specific modifications.*
