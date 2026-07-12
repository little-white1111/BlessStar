// Package adapter_mock 自动生成于 BlessStar 配置 Mock
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 商品管理
// 专为单元测试设计 — 固定返回值

package adapter_mock

import (
	"context"
	"douyin-mall-go-template/ports"
)

// ProductConfigMock 商品管理 域配置的 Mock 实现（单元测试用）
type ProductConfigMock struct {
	StatusValuesFunc func(ctx context.Context) (string, error)
}

// NewProductConfigMock 创建默认 Mock（返回值按 manifest 默认值设定）
func NewProductConfigMock() ports.ProductConfig {
	return &ProductConfigMock{
		StatusValuesFunc: func(ctx context.Context) (string, error) {
			return "{\"1\":\"on_sale\",\"0\":\"off_sale\",\"-1\":\"deleted\"}", nil
		},
	}
}

func (m *ProductConfigMock) StatusValues(ctx context.Context) (string, error) {
	return m.StatusValuesFunc(ctx)
}

