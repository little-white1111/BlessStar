# BlessStar — 配置驱动应用运行时引擎

[![CI](https://github.com/little-white1111/BlessStar/actions/workflows/ci.yml/badge.svg)](https://github.com/little-white1111/BlessStar/actions/workflows/ci.yml)

BlessStar 是一个面向**财务运维配置管理**场景的配置驱动应用运行时引擎。它采用三层架构（**App → Adapter → Kernel**），将业务配置以标准化指令（IR）的形式注入内核，实现配置的全生命周期管理——从厂商格式归一化、门禁校验、原子持久化，到内核状态同步与热更新。

## 核心特性

- **三层解耦架构**：App SDK（业务层）→ Adapter（编排层）→ Kernel（内核层），单向依赖，分层可测
- **配置即指令**：业务配置经归一化后转为 IR（Intermediate Representation），由内核流水线执行
- **三阶段门禁**：`default_gate`（格式校验）→ `policy_gates`（业务策略）→ `custom_gates`（自定义逻辑）
- **原子持久化**：WAL + CAS 提交，支持 `PER_PATH`/`PER_BATCH` 两种持久化方案，掉电安全
- **双路径提交**：`AddMemPath`（内存字节，快速热更新）和 `AddFilePath`（文件 URI，持久化基线）
- **热更新**：配置变更后重新提交即可触发内核热更新，无需重启进程
- **厂商格式归一化**：支持将异构源（如 JSON 业务文件）归一化为标准 Config v1 格式
- **配置审计**：内存路径变更自动记录到审计日志（manifest + 快照队列，上限 5 个版本）

## 快速上手

```cpp
#include <bs/app/sdk/app_session.h>
#include <bs/app/sdk/config_reload_session.h>

// 一行启动
bs::app::AppSession session("/var/bless/manifest.json");
if (!session.ok()) return;

// 提交配置
bs::app::ConfigReloadSession cs(session.ctx());
cs.AddMemPath("approval/rules", json_data, json_len);
cs.AddPolicyGate({.type = bs::app::ScenarioType::ExpenseReimburse});

Report* r = cs.Commit();
if (bs_report_get_status(r) != REPORT_STATUS_SUCCESS) {
    char* j = bs_report_to_json(r);
    fprintf(stderr, "FAIL: %s\n", j);
    free(j);
}
```

完整示例见 [`docs/QUICKSTART.md`](docs/QUICKSTART.md) 和 [`app/sdk/test/BsRealBizFullChainTest.cpp`](app/sdk/test/BsRealBizFullChainTest.cpp)。

## 架构概览

```
┌──────────────────────────────────────────────┐
│              App Layer (SDK)                  │
│   AppSession · ConfigReloadSession            │
│   VendorConfigNormalizer · ScenarioPolicy      │
│   MemAuditLog                                 │
└──────────────────┬───────────────────────────┘
                   │ 标准化 IR + Report
                   ▼
┌──────────────────────────────────────────────┐
│            Adapter Layer (编排)                │
│   ReloadBatchController · Gate Chain          │
│   Parser · WAL · Persist Store · Watch        │
└──────────────────┬───────────────────────────┘
                   │ 纯净 IR
                   ▼
┌──────────────────────────────────────────────┐
│            Kernel Layer (内核)                 │
│   ConfigManager · Pipeline · Executor Pool    │
│   StateBus · EventBus · Registry · IO         │
└──────────────────────────────────────────────┘
```

## 文档导航

| 文档 | 说明 |
|------|------|
| [快速开始](docs/QUICKSTART.md) | 5 分钟上手 BlessStar |
| [架构总览](docs/ARCHITECTURE_OVERVIEW.md) | 三层架构与核心概念详解 |
| [API 参考](docs/API_REFERENCE.md) | 所有 public 类/函数/枚举的详细说明 |
| [配置格式](docs/CONFIG_FORMAT.md) | BlessStar Config v1 JSON 格式规范 |
| [厂商归一化](docs/VENDOR_NORMALIZE.md) | 使用 VendorConfigNormalizer 将异构配置归一化为标准格式 |
| [场景策略](docs/SCENARIO_POLICY.md) | ScenarioPolicy 与自定义门禁 |
| [Report 解读](docs/REPORT_GUIDE.md) | 理解 Commit 执行结果 |
| [已知限制](docs/KNOWN_LIMITATIONS.md) | 当前版本限制与未来 Roadmap |

## 构建

```bash
# CMake 构建
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake

# 运行全量测试
ctest --test-dir build/cmake --output-on-failure

# 或用构建脚本
python tools/build/build.py --release
```

## 项目状态

BlessStar 目前处于 **MVP 阶段（核心稳定与商用加固）**，核心功能完成度约 85%：

- ✅ Kernel 内核（IR/pipeline/report/registry/state/IO/Runtime）
- ✅ Adapter 编排（parser/orchestration/persistence/watch/gates）
- ✅ App SDK（10 个 public API 全部实现并通过全链路测试）
- 🟡 文档正在持续补齐
- 🟡 Docker 构建环境标准化
- 🟡 CI sanitizer 全绿

更详细的实现进度见 [`架构方案选择记录.md`](架构方案选择记录.md) 跨日汇总索引。
