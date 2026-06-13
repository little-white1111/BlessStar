# 快速开始

> 参考：React 的 "Get Started in 5 Minutes" 风格——最小可行示例驱动

本文档引导你在 5 分钟内完成第一个 BlessStar 配置提交。读完本文后，你将了解：如何初始化 BlessStar 运行时、如何提交配置、如何检查结果。

## 前置条件

- C++17 编译器（GCC 12+ / Clang 15+ / MSVC 2022+）
- CMake 3.20+

## 1. 构建 BlessStar

```bash
# 克隆仓库
git clone https://github.com/little-white1111/BlessStar.git
cd BlessStar

# 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## 2. 最小示例：提交内存配置

创建一个 `demo.cpp`：

```cpp
#include <cstdio>
#include <cstdint>
#include <cstring>

#include <bs/app/sdk/app_session.h>
#include <bs/app/sdk/config_reload_session.h>

int main() {
    // === 1. 启动 BlessStar 运行时 ===
    // "一行启动"：自动完成 ctx_create → bootstrap → freeze → open_io → open_store
    bs::app::AppSession session("/tmp/bless_demo.manifest");
    if (!session.ok()) {
        fprintf(stderr, "BlessStar 启动失败\n");
        return 1;
    }

    // === 2. 创建配置提交会话 ===
    bs::app::ConfigReloadSession cs(session.ctx());

    // === 3. 添加配置（内存字节） ===
    const char* config_json = R"({
        "kernel_version": "0.3.0",
        "adapter_version": "0.3.0",
        "instructions": [
            {
                "type": "test",
                "name": "demo-config",
                "metadata": {
                    "key": "value"
                }
            }
        ]
    })";

    const uint8_t* data = reinterpret_cast<const uint8_t*>(config_json);
    size_t         len  = strlen(config_json);

    // AddMemPath: 将配置数据以内存方式提交
    // 第一个参数是逻辑 key，用于后续查询
    cs.AddMemPath("demo/my-config", data, len);

    // === 4. 提交配置 ===
    Report* r = cs.Commit();

    // === 5. 检查结果 ===
    if (bs_report_get_status(r) == REPORT_STATUS_SUCCESS) {
        printf("配置提交成功!\n");
        
        // 通过逻辑 key 查询配置状态
        ConfigState state;
        cs.GetConfig("demo/my-config", &state);
        printf("状态: %s\n", state == CONFIG_STATE_ACTIVE ? "ACTIVE" : "其他");
    } else {
        char* json = bs_report_to_json(r);
        fprintf(stderr, "配置提交失败:\n%s\n", json);
        free(json);
    }

    // === 6. 取走 Report 所有权并释放 ===
    Report* taken = cs.TakeReport();
    if (taken) bs_report_destroy(taken);

    // session 析构自动清理 BlessStar 运行时
    return 0;
}
```

## 3. 编译运行

将 `demo.cpp` 放在仓库根目录，与 BlessStar 一起编译：

```cpp
// CMakeLists.txt 中追加
add_executable(demo demo.cpp)
target_link_libraries(demo PRIVATE bs_app_sdk)
```

或在现有构建中直接编译：

```bash
g++ -std=c++17 demo.cpp -I app/sdk/include \
    -L build/lib -lbs_app_sdk -lpthread \
    -o demo

./demo
```

预期输出：

```
BlessStar 启动成功
配置提交成功!
状态: ACTIVE
```

## 4. 下一步：文件配置 + 热更新

大多数业务场景中，配置来自文件系统。使用 `AddFilePath` 来提交文件配置：

```cpp
// 提交文件配置
cs.AddFilePath("file:///etc/bless/expense_rules.json");

// 设置业务场景门禁
cs.AddPolicyGate({
    .type = bs::app::ScenarioType::ExpenseReimburse,
    .tenant = "tenant-a",
    .allow_hot_reload = true
});

Report* r = cs.Commit();
```

**热更新**：修改文件内容后，再次 `AddFilePath(uri) + Commit()` 即可触发 Kernel 热更新，无需重启进程。

## 5. metadata 消费

配置中的 `instructions.metadata` 字段可携带业务元数据（科目代码、税率阈值、审批链配置等）。`ConfigSessionReader` 提供结构化方式消费，无需手动 JSON 解析。

```cpp
#include <bs/app/sdk/config_session_reader.h>

void demo_metadata(bs::app::AppSession& session) {
    // 从 AppSession 构造 Reader（生命周期独立于提交流程）
    bs::app::ConfigSessionReader reader(session.ctx());

    // 假设之前提交过：AddMemPath("mycfg", json_data, len)
    const IRInstruction* instr = reader.GetInstruction(
        "mem://mycfg", "vat-rate");

    if (instr && instr->metadata) {
        // 直接读取 metadata，零二次解析开销
        const char* code = bs_ir_instruction_get_metadata(instr, "subject_code");
        const char* rate = bs_ir_instruction_get_metadata(instr, "tax_rate");
        printf("科目: %s, 税率: %s%%\n", code, rate);
    }
}
```

优势：

- **零额外解析**：读取 Gate 已校验通过的 IRInstructionList，不重复解析 JSON
- **线程安全**：内部 mutex 保护，可从 Watch callback 中安全调用
- **热更新感知**：自动版本号比对，配置变更后返回最新数据
- **FILE/MEM 统一**：MEM 路径和 FILE 路径均可通过 Reader 查询

## 6. 完整示例

完整的业务场景示例（含厂商格式归一化、Watch 通知、多路原子提交等）见：

- **全链路业务测试**：`app/sdk/test/BsRealBizFullChainTest.cpp`（8 个场景，含 metadata 消费）
- **ConfigReloadSession 单元测试**：`app/sdk/test/ConfigReloadSessionTest.cpp`

## 7. 常见问题

**Q: `AppSession` 的 manifest 路径是什么？**
A: 持久化存储的元数据文件路径。如果传 `nullptr`，持久化功能不可用，但只使用 `AddMemPath` 的场景不受影响。

**Q: `AddMemPath` 和 `AddFilePath` 的区别？**
A: `AddMemPath(key, data, len)` 提交内存数据，不写入磁盘，重启后丢失（有审计日志）。`AddFilePath(uri)` 提交文件数据，会原子持久化到磁盘，重启后自动恢复。

**Q: `Commit()` 能混合提交 MEM 和 FILE 吗？**
A: 可以。Session 内部会自动按"先 FILE 再 MEM"的顺序执行。FILE 阶段失败时整体回滚，MEM 阶段不会执行，保证原子性。

**Q: 如何检查配置是否生效？**
A: 三选一：① 检查 `Commit()` 返回的 `Report` 状态；② 通过 `GetConfig(key)` 查询 ConfigManager 中的状态；③ 注册 Watch callback 接收状态变更通知。
