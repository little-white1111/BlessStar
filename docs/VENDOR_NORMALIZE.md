# 厂商格式归一化指南

> 参考风格：React 的 "Recipes" —— 场景驱动，每一步都有代码示例

## 为什么需要归一化？

在生产环境中，配置的来源是异构的：

- 核心系统输出 JSON 格式
- 上游 ERP 可能输出 XML
- 金蝶/用友等财务系统有各自专有格式

BlessStar 的核心流水线只消费一种格式——**BlessStar Config v1**。因此，所有异构的配置源必须先经过**归一化（Normalize）**步骤，转换为标准 v1 格式后，才能提交给 BlessStar 引擎。

## 归一化流程

```
厂商文件 (.json / .xml / 专有格式)
       │
       ▼  VendorConfigNormalizer
          ├─ 读取文件
          ├─ 识别业务场景
          ├─ 提取指令和数据
          └─ 转换为 v1 JSON 字节
       │
       ▼  NormalizeResult.v1_bytes
       │
       ▼  ConfigReloadSession.AddMemPath() / AddFilePath()
```

## 基础用法

### 1. 从厂商文件归一化到内存字节

```cpp
#include <bs/app/sdk/vendor_config_normalizer.h>

bs::app::NormalizeResult result;
bool ok = bs::app::NormalizeVendorConfig(
    bs::app::VendorFormat::GenericBusinessJson,  // 厂商格式
    "/data/vendor/expense_config.json",           // 厂商文件路径
    &result);                                     // 输出

if (ok && result.ok) {
    // result.v1_bytes 包含标准化的 v1 配置
    printf("归一化成功! 场景: %s\n", result.scenario.c_str());
    printf("v1 大小: %zu 字节\n", result.v1_bytes.size());
    
    // 直接提交给 BlessStar
    session.AddMemPath("expense/rules",
                       result.v1_bytes.data(),
                       result.v1_bytes.size());
} else {
    fprintf(stderr, "归一化失败: %s\n", result.error.c_str());
}
```

### 2. 从厂商文件到临时文件 URI

如果希望配置走持久化路径（重启后可用），可以使用 `NormalizeFileToTempUri`：

```cpp
#include <bs/app/sdk/vendor_reload_facade.h>

std::string uri;
bs::app::NormalizeResult result;

bool ok = bs::app::NormalizeFileToTempUri(
    bs::app::VendorFormat::GenericBusinessJson,
    "/data/vendor/expense_config.json",
    "/tmp/bless_vendor",     // 临时文件输出目录
    &uri,                    // 输出: file:///tmp/bless_vendor/xxx.json
    &result);

if (ok && result.ok) {
    printf("临时文件 URI: %s\n", uri.c_str());
    printf("场景: %s\n", result.scenario.c_str());
    
    // 走文件持久化路径提交
    session.AddFilePath(uri.c_str());
}
```

### 3. 完整业务场景：厂商文件 → 归一化 → 提交 → 验证

```cpp
#include <bs/app/sdk/app_session.h>
#include <bs/app/sdk/config_reload_session.h>
#include <bs/app/sdk/vendor_config_normalizer.h>

int submit_vendor_config(const char* vendor_path) {
    // 启动 BlessStar
    bs::app::AppSession session("/var/bless/manifest.json");
    if (!session.ok()) return -1;
    
    // 归一化
    bs::app::NormalizeResult norm;
    if (!bs::app::NormalizeVendorConfig(
            bs::app::VendorFormat::GenericBusinessJson,
            vendor_path, &norm) || !norm.ok) {
        fprintf(stderr, "归一化失败: %s\n", norm.error.c_str());
        return -1;
    }
    
    // 提交
    bs::app::ConfigReloadSession cs(session.ctx());
    cs.AddMemPath("vendor-config", norm.v1_bytes.data(), norm.v1_bytes.size());
    
    // 设置场景门禁
    bs::app::ScenarioPolicy policy;
    policy.type = bs::app::ScenarioType::ExpenseReimburse;
    policy.tenant = "tenant-a";
    cs.AddPolicyGate(policy);
    
    Report* r = cs.Commit();
    if (bs_report_get_status(r) != REPORT_STATUS_SUCCESS) {
        char* j = bs_report_to_json(r);
        fprintf(stderr, "提交失败: %s\n", j);
        free(j);
        return -1;
    }
    
    printf("配置提交成功!\n");
    Report* taken = cs.TakeReport();
    if (taken) bs_report_destroy(taken);
    return 0;
}
```

## NormalizeResult 结构

```cpp
struct NormalizeResult {
    bool                      ok;              // 归一化是否成功
    std::vector<std::uint8_t> v1_bytes;        // 归一化后的 v1 字节
    std::string               source_vendor;   // 来源厂商（如 "generic_business"）
    std::string               scenario;        // 识别出的业务场景（如 "expense_reimburse"）
    std::string               error;           // 失败时的详细错误信息
};
```

## 支持的厂商格式

### GenericBusinessJson（当前版本）

接受遵循特定 JSON 结构的业务配置文件，自动提取：

- `kernel_version` / `adapter_version`：校验与对齐
- `instructions`：将业务指令映射为 IR 格式
- `metadata`：提取业务参数作为 key-value 对
- 场景识别：根据配置内容自动推断业务场景

示例输入文件见：`app/sdk/test/fixtures/vendor_generic_business_good.json`

### 金蝶/用友（二期，VP-5）

金蝶和用友等国产财务系统的专有格式支持计划在第二阶段实现。当前版本只预留了枚举值。

```cpp
enum class VendorFormat {
    GenericBusinessJson = 0
    // Yonyou, Kingdee: phase 2 (VP-5)
};
```

## 边界情况处理

| 场景 | 行为 |
|------|------|
| 空指令数组 `"instructions": []` | 合法，表示不执行任何指令 |
| 缺少必需字段（`kernel_version` 等） | 解析失败，`result.ok = false`，`result.error` 包含原因 |
| 非 UTF-8 编码输入 | 解析失败（UTF-8 由底层 parser 校验） |
| 超大配置（>1MiB） | 读取/解析成功，但需自行评估性能影响 |
| 重复 key | 解析失败（Kernel 不允许重复 key） |
