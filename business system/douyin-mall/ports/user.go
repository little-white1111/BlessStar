// Package ports 自动生成于 BlessStar 配置端口-适配器
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 用户管理
// 请勿手动修改 — 由 blessstar-codegen 自动生成
// Source: manifest.json

package ports

import (
	"context"
)

// UserConfig 用户管理 域配置接口
// 对应 domain: "用户管理"
// 禁止直接 import BlessStar SDK — 请通过此接口访问配置
type UserConfig interface {

	// // 用户角色 (user.role.values)
// 描述: 平台用户角色枚举定义，决定用户在系统中的权限等级。
// 建议值: 当前支持 user, admin。如需扩展可新增如 merchant,客服等角色
// 类型: ENUM
	// Returns: string
	RoleValues(ctx context.Context) (string, error)

	// // 用户状态 (user.status.values)
// 描述: 用户账号状态枚举定义。1=正常，0=停用，-1=已删除。
// 建议值: 当前支持 active(1)/inactive(0)/deleted(-1)。建议保留-1作为删除标记
// 类型: ENUM
	// Returns: string
	StatusValues(ctx context.Context) (string, error)

	// // 默认角色 (user.registration.default_role)
// 描述: 新用户注册时默认分配的角色。
// 建议值: 必须为 user.role.values 中定义的值，当前有效值：user, admin
// 类型: STR
	// Returns: string
	RegistrationDefaultRole(ctx context.Context) (string, error)

	// // 默认状态 (user.registration.default_status)
// 描述: 新用户注册成功后的默认账号状态。1=正常激活。
// 建议值: 必须为 user.status.values 中定义的值。1=正常激活，0=停用（需额外激活）
// 类型: I32
	// Returns: int32
	RegistrationDefaultStatus(ctx context.Context) (int32, error)

	// // 用户名最小长度 (user.validation.username_min_length)
// 描述: 用户名允许的最小字符数。
// 建议值: 1~20，推荐步长1。建议不低于2
// 类型: I32
	// Returns: int32
	ValidationUsernameMinLength(ctx context.Context) (int32, error)

	// // 用户名最大长度 (user.validation.username_max_length)
// 描述: 用户名允许的最大字符数，与数据库varchar(50)一致。
// 建议值: 10~50，推荐步长5。建议不超过数据库字段长度（当前50）
// 类型: I32
	// Returns: int32
	ValidationUsernameMaxLength(ctx context.Context) (int32, error)

	// // 密码最小长度 (user.validation.password_min_length)
// 描述: 密码允许的最小字符数。
// 建议值: 6~20，推荐步长1。建议不低于6
// 类型: I32
	// Returns: int32
	ValidationPasswordMinLength(ctx context.Context) (int32, error)

	// // 密码最大长度 (user.validation.password_max_length)
// 描述: 密码允许的最大字符数。
// 建议值: 20~72，推荐步长5。建议不超过72（bcrypt算法输入上限）
// 类型: I32
	// Returns: int32
	ValidationPasswordMaxLength(ctx context.Context) (int32, error)

	// // 邮箱必填 (user.validation.email_required)
// 描述: 注册时是否强制要求填写邮箱地址。
// 建议值: true/false。true=必填，false=选填
// 类型: BOOL
	// Returns: bool
	ValidationEmailRequired(ctx context.Context) (bool, error)
}
