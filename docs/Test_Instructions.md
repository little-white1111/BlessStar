# Kernel State Module - Test Instructions

## 📋 测试概览

当前已为 `kernel/state` 模块编写了完整单元测试，覆盖核心功能。

| 测试文件 | 位置 | 说明 |
|----------|------|------|
| StateMachineTest.cpp | kernel/state/test/ | 状态机测试 |
| StateBusTest.cpp | kernel/state/test/ | 状态总线测试 |
| AllTests.cpp | kernel/state/test/ | 所有测试统一入口 |

---

## 📦 测试用例覆盖

### 1. StateMachineTest.cpp (6个测试)

| 测试 | 内容 |
|------|------|
| test_StateMachine_CreateDestroy | 创建/销毁状态机，验证初始状态和版本 |
| test_StateMachine_ValidTransitions | 验证所有有效状态转换 |
| test_StateMachine_InvalidTransitions | 验证无效转换被阻止 |
| test_StateMachine_CanTransition | 验证 CanTransition 查询 |
| test_StateMachine_Callback | 验证状态转换回调 |
| test_StateMachine_ToString | 验证状态名称字符串转换 |

### 2. StateBusTest.cpp (6个测试)

| 测试 | 内容 |
|------|------|
| test_StateBus_CreateDestroy | 创建/销毁状态总线 |
| test_StateBus_SetGetState | 设置/获取配置状态和数据 |
| test_StateBus_VersionIncrement | 验证版本号单调递增 |
| test_StateBus_GetSnapshot | 获取配置数据快照 |
| test_StateBus_GetAllEntries | 批量获取所有配置条目 |
| test_StateBus_NullData | 验证null数据正确处理 |

---

## 🏗️ 构建和运行测试

### 前置条件
- CMake 3.20+（已安装）
- C++17编译器（Visual Studio 2019+ / MinGW-w64 g++ 11+ / Clang 13+）

### 步骤1：配置构建

```bash
# Windows (Visual Studio)
cmake -B build -S . -G "Visual Studio 17 2022" -DBLESSSTAR_ENABLE_PURITY_CHECK=OFF

# Windows (MinGW Makefiles)
cmake -B build -S . -G "MinGW Makefiles" -DBLESSSTAR_ENABLE_PURITY_CHECK=OFF

# Linux/macOS
cmake -B build -S . -DBLESSSTAR_ENABLE_PURITY_CHECK=OFF
```

### 步骤2：编译

```bash
cmake --build build --config Release
```

### 步骤3：运行测试

```bash
# 单独运行状态机测试
./build/bs_test_state_machine  # Linux/macOS
.\build\Release\bs_test_state_machine.exe  # Windows

# 单独运行状态总线测试
./build/bs_test_state_bus
.\build\Release\bs_test_state_bus.exe

# 运行所有测试
./build/bs_test_state_all
.\build\Release\bs_test_state_all.exe
```

---

## 📊 预期输出

### StateMachineTest.cpp
```
=== StateMachine Tests ===
test_StateMachine_CreateDestroy: PASS
test_StateMachine_ValidTransitions: PASS
test_StateMachine_InvalidTransitions: PASS
test_StateMachine_CanTransition: PASS
test_StateMachine_Callback: PASS
test_StateMachine_ToString: PASS
=== All StateMachine Tests PASSED! ===
```

### StateBusTest.cpp
```
=== StateBus Tests ===
test_StateBus_CreateDestroy: PASS
test_StateBus_SetGetState: PASS
test_StateBus_VersionIncrement: PASS
test_StateBus_GetSnapshot: PASS
test_StateBus_GetAllEntries: PASS
test_StateBus_NullData: PASS
=== All StateBus Tests PASSED! ===
```

---

## 🛠️ 如果没有C++编译器

如果你当前没有C++编译器，推荐安装以下之一：

### 推荐方案（Windows）
1. **Visual Studio 2022 Community**（免费）
   - 下载：https://visualstudio.microsoft.com/downloads/
   - 安装时勾选 "Desktop development with C++"

2. **MinGW-w64**（轻量）
   - 下载：https://github.com/niXman/mingw-builds-binaries/releases
   - 解压后添加 bin 目录到 PATH

---

## 📝 测试架构说明

| 组件 | 语言 | 说明 |
|------|------|------|
| bs_kernel_state 库 | C++17 | 状态管理核心库 |
| bs_test_state_machine | C++17 | 状态机测试程序 |
| bs_test_state_bus | C++17 | 状态总线测试程序 |
| bs_test_state_all | C++17 | 所有测试集成程序 |

---

## ✅ 已完成工作

- ✅ 所有C++实现代码（StateMachine、StateBus、EventBus、WatchManager、TemporaryState）
- ✅ 完整单元测试（12个测试用例）
- ✅ CMakeLists.txt 配置
- ✅ 项目修改记录完整更新
- ✅ 头文件保持C ABI兼容

---

## 📌 下一步

当你安装好C++编译器后，就可以按照上面的步骤编译和运行测试了！
