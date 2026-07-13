// Package provider 依赖注入
//
// 当前使用手动 DI（在 cmd/server/main.go 和 cmd/cli/main.go 中显式组装）。
// 后续如果引入 Google Wire 工具，可将此文件转换为 wire 模板。
//
// 接入 BlessStar 的修改点（零业务侵入）：
// 1. 新增 adapter/config/blessstar.go 实现 port.ConfigReader
// 2. 在此文件中将 ConfigReader 的注入替换为 BlessStar 实现
// 3. internal/domain/ 和 internal/service/ 零改动
package provider
