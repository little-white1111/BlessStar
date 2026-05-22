# BlessStar（MVP 第1天：纯净内核骨架）

本仓库面向“财务运维配置管理中间件 MVP”，当前交付为**第1天最小工程骨架**：以“多模块纯净内核 + 适配层”组织代码，并在构建/CI 层落地最小可执行的**污染即阻断**门禁。

## 1. 目标与定位（纯净内核）

- **内核 = CPU/模具**：对外接口与核心逻辑不可被外部直接修改；外部输入只能通过标准化指令（IR）进入。
- **高内聚低耦合**：内核以模块方式拆分，但对外作为一个整体内核运作。
- **污染即阻断**：任何模块被污染立即报错阻断；只修污染模块，不连坐其他模块。

## 2. 模块化与单线流水线

本仓库采用“Spring Cloud 多模块思想”的工程拆分，但在内核内部以**单线流水线**组织依赖：

- 下游模块仅依赖上游模块的**输出指令/IR**。
- **严禁反向依赖**与横向共享状态。
- `NextTarget/NextAction` 设计目标是能直接命中“当前下游/当前未完成模块”，用于快速隔离与修复。

目录（第1天最小骨架）：

- `kernel/`：纯净内核（多模块）
  - `kernel/ir/`：IR 输入抽象（唯一纯净输入）
  - `kernel/pipeline/`：单线流水线编排（仅依赖上游 IR）
  - `kernel/report/`：执行情况报告与 NextTarget/NextAction（唯一纯净输出）
- `adapter/`：适配层（将外部世界翻译成 IR；执行内核 NextAction/Plan 并回填下一轮输入）
- `factory/`：出厂包（只读消费区，主仓只同步指定版本，不做写回修改）
- `tools/`：门禁脚本（manifest 校验、辅助生成）

## 3. 唯一纯净 I/O（元数据组）

- **唯一输入（IR）**：适配层生成规范的“内核输入配置文件/IR”，只包含与内核模块有关的抽象数据，作为内核唯一纯净输入。
- **唯一输出**：内核输出
  - 执行情况报告（Report）
  - 最小 `NextTarget/NextAction`
  - （可选）抽象 Plan（第1天占位：不实现）
- 适配层解析输出后结合当前环境做二次翻译并执行；反馈进入下一轮输入必须严格白名单，避免环境噪声回流。

## 4. 治理与修复（供应链式出厂纯度）

- `Source/*`：上游源码包（独立仓库维护更新，**唯一可写、唯一标准源**）。本仓库第1天**不修改/不运行**其中内容。
- `factory/`：出厂包（主仓只同步指定版本，**不做写回修改**）。
- 纯度校验：通过版本锚点（tag/sha）+ manifest（文件清单 + hash）做一致性验证；不一致即视为污染并阻断。

门禁脚本：

- `tools/purity/verify_manifest.py`：校验 `factory/manifest.sha256` 与实际文件一致性（CI 中执行）
- `tools/purity/generate_manifest.py`：生成 manifest（用于“源码包→出厂包同步”流程；第1天保留为工具占位）

## 5. 构建与 CI

第1天只提供**可 configure 通过**的最小脚手架，并可完成最小编译：

- CMake：`CMakeLists.txt`
- Meson：`meson.build`
- GitHub Actions：`.github/workflows/ci.yml`（manifest 校验 + **clang-format**（仅 Linux）+ CMake **`-DBLESSSTAR_TREAT_WARNINGS_AS_ERRORS=ON`** + 构建 + **CTest**；Meson 另 job）

本地若要对齐 CI 的「警告即错误」：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBLESSSTAR_TREAT_WARNINGS_AS_ERRORS=ON
cmake --build build
```

### C/C++ 代码风格（clang-format）

- **勿**对 `.py` / `.sh` 运行 `clang-format`（会破坏 shebang 与语法）。
- Windows 推荐：`.\tools\dev\format_cpp_sources.ps1`（可选 `-ClangFormat` 指向本机 `clang-format.exe`）。
- Linux/macOS：可用 `git ls-files` 白名单扩展名 + `clang-format --dry-run --Werror`（与 CI 一致），或对上述目录手动 `clang-format -i`。

## 6. 规范

- C++ 编码规范初稿：`docs/cpp-coding-standards.md`
- Git 提交规范：`docs/git-commit-convention.md`

## 7. 快速开始（本地）

### CMake

```bash
cmake -S . -B build/cmake
cmake --build build/cmake
```

### Meson

```bash
meson setup build/meson
meson compile -C build/meson
```

### 纯度门禁（manifest 校验）

```bash
python tools/purity/verify_manifest.py verify --kernel factory --manifest factory/manifest.sha256
```
