// Package adapter_mock 自动生成于 BlessStar 配置 Mock
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 订单管理
// 专为单元测试设计 — 固定返回值

package adapter_mock

import (
	"context"
	"douyin-mall-go-template/ports"
)

// OrderConfigMock 订单管理 域配置的 Mock 实现（单元测试用）
type OrderConfigMock struct {
	StatusValuesFunc func(ctx context.Context) (string, error)
}

// NewOrderConfigMock 创建默认 Mock（返回值按 manifest 默认值设定）
func NewOrderConfigMock() ports.OrderConfig {
	return &OrderConfigMock{
		StatusValuesFunc: func(ctx context.Context) (string, error) {
			return "{\"0\":\"pending_payment\",\"1\":\"paid\",\"2\":\"shipped\",\"3\":\"delivered\",\"4\":\"completed\",\"-1\":\"cancelled\"}", nil
		},
	}
}

func (m *OrderConfigMock) StatusValues(ctx context.Context) (string, error) {
	return m.StatusValuesFunc(ctx)
}

