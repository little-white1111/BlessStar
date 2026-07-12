// Package ports 自动生成于 BlessStar 配置端口-适配器
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 订单管理
// 请勿手动修改 — 由 blessstar-codegen 自动生成
// Source: manifest.json

package ports

import (
	"context"
)

// OrderConfig 订单管理 域配置接口
// 对应 domain: "订单管理"
// 禁止直接 import BlessStar SDK — 请通过此接口访问配置
type OrderConfig interface {

	// // 订单状态 (order.status.values)
// 描述: 订单生命周期状态枚举。
// 建议值: 当前状态流转：0→1→2→3→4（正向），任何状态均可→-1（取消）。新增状态（如售后中、退款中）需定义流转规则
// 类型: ENUM
	// Returns: string
	StatusValues(ctx context.Context) (string, error)
}
