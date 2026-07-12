// Package ports 自动生成于 BlessStar 配置端口-适配器
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 支付管理
// 请勿手动修改 — 由 blessstar-codegen 自动生成
// Source: manifest.json

package ports

import (
	"context"
)

// PaymentConfig 支付管理 域配置接口
// 对应 domain: "支付管理"
// 禁止直接 import BlessStar SDK — 请通过此接口访问配置
type PaymentConfig interface {

	// // 支付方式 (payment.type.values)
// 描述: 平台支持的支付方式枚举。
// 建议值: 当前支持：1=支付宝，2=微信支付，3=信用卡。新增需评估支付通道成本和技术对接周期
// 类型: ENUM
	// Returns: string
	TypeValues(ctx context.Context) (string, error)

	// // 支付记录状态 (payment.record_status.values)
// 描述: 支付记录处理状态枚举。
// 建议值: 当前支持：0=待处理→1=成功/2=失败，3=已退款（从成功状态转入）
// 类型: ENUM
	// Returns: string
	RecordStatusValues(ctx context.Context) (string, error)
}
