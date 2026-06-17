// ============================================================
// auth.h — 本地认证模块（预留位置）
// ============================================================
//
// 本模块为 BlessStar Config Editor 预留的本地认证扩展点。
// MVP 阶段不实现实际认证逻辑，仅预留目录与接口占位。
//
// 后续可在此处添加：
//   - 本地设备绑定认证
//   - 离线许可证校验
//   - 配置文件的访问权限控制
//   - 密钥/证书管理
//
// 使用示例（MVP 后）：
//   #include "auth/auth.h"
//   bs::auth::AuthContext ctx;
//   if (!ctx.verify()) { /* 拒绝访问 */ }
//
// ============================================================
#pragma once

namespace bs {
namespace auth {

// MVP 阶段：认证上下文为空结构，不做实际校验
struct AuthContext {
    // bool verified = false;   // 解锁后启用
    // std::string device_id;   // 设备绑定
};

}  // namespace auth
}  // namespace bs
