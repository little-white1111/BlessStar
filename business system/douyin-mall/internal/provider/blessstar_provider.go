// Package douyin_mall 自动生成于 BlessStar 配置依赖注入
// 业务系统: 抖音商城 (douyin-mall)
// 请勿手动修改 — 由 blessstar-codegen 自动生成

package douyin_mall

import (
	"context"
	"errors"
	"log"

	"douyin-mall-go-template/ports"
	adapter_blessstar "douyin-mall-go-template/internal/adapters/blessstar"
)

// nilReader 是一个始终返回错误的 ConfigReader 实现。
// 当用户传入 nil reader 时使用，确保 adapter 不会空指针，
// 从而顺利走三阶段降级的 Cache→hardcoded default 路径。
type nilReader struct{}

func (n *nilReader) Get(_ context.Context, _ string) (interface{}, error) {
	return nil, errors.New("[BlessStar] ConfigReader is nil: using hardcoded defaults")
}

// Adapters 持有所有域的配置适配器
type Adapters struct {
	ProductConfig ports.ProductConfig
	CorsConfig    ports.CorsConfig
	PaymentConfig ports.PaymentConfig
	UserConfig    ports.UserConfig
	OrderConfig   ports.OrderConfig
	AuthConfig    ports.AuthConfig
	ReviewConfig  ports.ReviewConfig
}

// DefaultAdapters 是全局默认适配器实例，由 InitAdapters 初始化。
var DefaultAdapters *Adapters

// InitAdapters 初始化所有域配置适配器，非阻塞启动。
// reader 可传入 nil，此时全部适配器走 3 阶段降级的 Cache→Default 路径。
func InitAdapters(reader ports.ConfigReader) {
	if reader == nil {
		log.Println("[BlessStar] ConfigReader is nil, adapters will use last-known-good cache and hardcoded defaults (3-stage fallback)")
		reader = &nilReader{}
	}
	DefaultAdapters = &Adapters{
		ProductConfig: adapter_blessstar.NewProductConfigAdapter(reader),
		CorsConfig:    adapter_blessstar.NewCorsConfigAdapter(reader),
		PaymentConfig: adapter_blessstar.NewPaymentConfigAdapter(reader),
		UserConfig:    adapter_blessstar.NewUserConfigAdapter(reader),
		OrderConfig:   adapter_blessstar.NewOrderConfigAdapter(reader),
		AuthConfig:    adapter_blessstar.NewAuthConfigAdapter(reader),
		ReviewConfig:  adapter_blessstar.NewReviewConfigAdapter(reader),
	}
}

// ProvideBlessStarAdapters 一行实例化所有域配置适配器
// 注入 ConfigReader 后返回所有域的 Port 实现
// reader 由业务方提供（可为 CachedReader、HTTPReader、EnvReader 等实现）
func ProvideBlessStarAdapters(reader ports.ConfigReader) *Adapters {
	return &Adapters{
		ProductConfig: adapter_blessstar.NewProductConfigAdapter(reader),
		CorsConfig:    adapter_blessstar.NewCorsConfigAdapter(reader),
		PaymentConfig: adapter_blessstar.NewPaymentConfigAdapter(reader),
		UserConfig:    adapter_blessstar.NewUserConfigAdapter(reader),
		OrderConfig:   adapter_blessstar.NewOrderConfigAdapter(reader),
		AuthConfig:    adapter_blessstar.NewAuthConfigAdapter(reader),
		ReviewConfig:  adapter_blessstar.NewReviewConfigAdapter(reader),
	}
}
