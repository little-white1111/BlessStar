# CI 构建脚本迁移阶段规划（第24天）

## Phase 1（已完成 — 第24天）

**目标**：cmake (ubuntu) job 使用 `build.py --print-cmake-args` 替代手写 cmake 命令。

**变更**：

- `ci.yml` cmake (ubuntu) 的 Configure 步骤改为 `eval "$(python tools/build/build.py --release --print-cmake-args --build-dir=build_ci_test)"`
- Windows 和其他 job 保持不变

**验收标准**：

- CI cmake (ubuntu) 功能与之前一致
- `build.py --release --print-cmake-args --format=json` 输出 `{build_dir, args, env}`

---

## Phase 2（已完成 — 第24天）

**目标**：Windows cmake job 也迁移至 `build.py --print-cmake-args`。

**变更**：

- `ci.yml` cmake (windows) 的 Configure 步骤改为通过 `build.py --release --print-cmake-args --format=json` 在 PowerShell 中解析并执行
- `detect.py` 新增 `is_msvc_available()` 辅助函数，在 Windows 上自动探测 MSVC（cl.exe）
- `detect.py` `probe_compiler()` 新增 Priority 3 Windows MSVC 自动探测分支
- `detect.py` `_compiler_family()` 新增 `"msvc"` 家族识别
- `detect.py` `_cxx_for_cc()` 对 MSVC（cl/cl.exe）返回相同编译器
- `detect.py` `resolve_compilers()` 对 MSVC 跳过版本检查
- `cmake.py` `build_cmake_args()` `allow_fallback` 路径在 Windows + MSVC 可用时使用 `cl.exe`

**验收标准**：

- `build.py --release --print-cmake-args --format=json --build-dir=build_ci_test` 在 Windows 上输出含 MSVC 的 cmake 命令
- `ci.yml` Configure (Windows) 步骤已迁移至 `build.py`
- Windows CI 构建无回归

---

## Phase 3（已完成 — 第24天）

**目标**：完全消除 ci.yml 中的手写 cmake 命令，统一通过 build.py 驱动全部构建流程。

**变更**：

- sanitizer job Configure 迁移至 `build.py --sanitize --print-cmake-args --build-dir=build-san`
- tsan job Configure 迁移至 `build.py --tsan --print-cmake-args --build-dir=build-tsan`
- `cmake.py` `BUILD_PRESETS` 中 sanitize 和 tsan 预设已包含 `BLESSSTAR_SANITIZER_CI=ON`
- `cmake.py` `_sanitizer_flags()` 正确设置 `CMAKE_C_FLAGS`、`CMAKE_CXX_FLAGS`、`CMAKE_EXE_LINKER_FLAGS`
- `cmake.py` `allow_fallback` 路径在 Linux + sanitize/tsan 预设时自动使用 clang/clang++
- 所有 Configure 步骤的 `--build-dir` 使用 CI 目录名（`build_ci_test`、`build-san`、`build-tsan`）

**验收标准**：

- `build.py --sanitize --print-cmake-args --format=json --build-dir=build-san` 输出与原手动 cmake 等价
- `build.py --tsan --print-cmake-args --format=json --build-dir=build-tsan` 输出与原手动 cmake 等价
- `ci.yml` 中所有 cmake Configure 步骤已替换（meson/docker 保持不动）
- `ci.yml` 中无手写 `cmake -S . -B` 命令

## 架构不变量

- **BLD-CI-EVAL-1**：CI 通过 eval `$(build.py --print-cmake-args)` 执行 cmake，build.py 是 cmake 参数的唯一真相源
- **BLD-CI-WIN-1**：Windows CI 通过 `--format=json` + PowerShell 解析，避免 shell-eval 兼容性问题
- **BLD-CI-MSVC-1**：`detect.py` 在 Windows 上自动探测 MSVC，不覆盖用户显式指定的编译器
- **BLD-CI-SAN-1**：sanitize/tsan 预设默认使用 clang（Linux），可通过 `--cc`/`--cxx` 覆盖
- **BLD-CI-DIR-1**：`--build-dir` 覆盖默认目录名，与 gate runner 契约路径保持一致