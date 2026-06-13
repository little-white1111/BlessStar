# 已知限制与 Roadmap

> 参考风格：React 的 "Limitations" + Kubernetes 的 "Feature Gates" —— 清晰列出当前不支持的内容和未来计划

## 当前版本

BlessStar 处于 **MVP 阶段（核心稳定与商用加固）**，版本号：`0.3.x`。

## 已知限制

### 功能限制

| 限制 | 说明 | 预计解决 |
|------|------|----------|
| **HTTP/HTTPS 配置来源** | `PathSource::kHttp` / `kHttps` 枚举已预留但未实现，传参会返回 `BS_ATTACH_ERR_NOT_IMPLEMENTED` | 二期 |
| **金蝶/用友格式归一化** | `VendorFormat` 中预留了 `Yonyou` / `Kingdee` 枚举但未实现 parser | 二期 (VP-5) |
| **异步 IO** | `IoBus` 目前为同步实现，无异步背压机制 | 二期 |
| **远程 Provider** | 只支持 `file://` 和 `mem://` 协议，无数据库/网络配置源 | 二期 |
| **跨进程/跨节点事件总线** | Watch 机制当前只支持单进程内的事件订阅 | 二期 |
| **CLI 工具** | 无生产 CLI 工具，bootstrap 和配置检查需通过代码调用 | 二期 |

### 工程限制

| 限制 | 说明 | 预计解决 |
|------|------|----------|
| **Docker 构建环境** | Dockerfile 和 docker-compose.yml 已创建但未完全集成到 CI | 第 25 天 |
| **ASan CI 全绿** | Sanitizer CI job 偶尔有告警，需进一步收敛 | 进行中 |
| **内存压力测试** | 72 小时长稳测试框架已就绪但 baseline 数据未全部收敛 | 进行中 |
| **4MiB 超大文件压测** | 大文件边界测试场景尚未覆盖 | 二期 |

### API 限制

| 限制 | 说明 |
|------|------|
| **线程安全** | `ConfigReloadSession` / `AppSession` / `MemAuditLog` 均不是线程安全的。所有调用必须在创建线程上进行。跨线程使用会触发 assert 检查 |
| **Session 不可重用跨线程** | 不允许将一个线程中创建的 Session 传递给另一个线程使用 |
| **kHttp/kHttps 传参报错** | `AddPath(uri, PathSource::kHttp)` 返回 `BS_ATTACH_ERR_NOT_IMPLEMENTED` |

## Roadmap

### 阶段 1：MVP 核心（当前，已完成 85%）

- [x] Kernel 内核（IR/pipeline/report/registry/state/IO/Runtime）
- [x] Adapter 编排（parser/gate/persist(WAL)/watch/log）
- [x] App SDK（ConfigReloadSession/AppSession 等 10 个 API）
- [x] 全链路测试（7 场景全部通过）
- [x] 用户体验收口（MEM/FILE 双路径、两阶段提交、审计日志）
- [x] 架构方案选择记录
- [ ] Docker 构建环境标准化
- [ ] 面向用户的开发文档 ← **当前在此**

### 阶段 2：商用加固（第 25～31 天，进行中）

- [ ] SDK 打包与发布（.deb / .rpm / CMake 包管理）
- [ ] CI 全绿 + Docker 集成
- [ ] 内存/压力/长稳测试最终收口
- [ ] API 文档正式定稿
- [ ] 三方厂商格式归一化（VP-5：金蝶/用友）

### 阶段 3：生态与扩展（二期）

- [ ] HTTP/HTTPS 配置来源支持
- [ ] 异步 IoBus + 背压
- [ ] 跨进程/跨节点事件总线
- [ ] CLI 管理工具
- [ ] 配置变更历史 UI
- [ ] 性能基线自动化

## 迁移与升级注意事项

### v0.2.x → v0.3.x

v0.3.x 引入了用户体验收口变更（第 24 天），包括 `AddMemPath`/`AddFilePath`、两阶段 Commit、审计日志。所有旧 API（`AddPath`/`AddUri`）**完全保留**，旧代码无需修改即可编译和运行。

| 旧 API | 新 API | 说明 |
|--------|--------|------|
| `AddPath(key, data, len)` | 完全保留 | 新增同等语义 `AddMemPath(key, data, len)` |
| `AddUri(uri)` | 完全保留 | 新增同等语义 `AddFilePath(uri)` |

### 从旧版本升级

```cpp
// v0.2.x 代码（完全兼容）
session.AddPath("key", data, len);
session.AddUri("file:///etc/config.json");

// v0.3.x 同样代码（完全兼容）+ 可选的增强写法
session.AddMemPath("key", data, len);   // 新方法，语义更清晰
session.AddFilePath("file:///etc/config.json");  // 新方法，语义更清晰
```

## 反馈与贡献

- **问题/建议**：请提 GitHub Issue
- **架构讨论**：见 `架构方案选择记录.md`
- **工程变更**：见 `项目修改记录.md`
