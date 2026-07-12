// Package adapter_blessstar 自动生成于 BlessStar 配置端口-适配器
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 评价管理
// 请勿手动修改 — 由 blessstar-codegen 自动生成

package adapter_blessstar

import (
	"context"
	"sync"

	"douyin-mall-go-template/ports"
)

// ReviewConfigAdapter 评价管理 域配置的 BlessStar 适配器
// 内置三阶段降级: ConfigReader实时查询 → LastKnownGood缓存 → 硬编码默认值
type ReviewConfigAdapter struct {
	reader          ports.ConfigReader
	lastKnownCache  sync.Map
	hardcodedDefaults  map[string]interface{}
}

// NewReviewConfigAdapter 创建 ReviewConfigAdapter 适配器实例
// reader 参数是配置读取器，由业务方注入（可为 CachedReader、HTTPReader 等实现）
func NewReviewConfigAdapter(reader ports.ConfigReader) ports.ReviewConfig {
	return &ReviewConfigAdapter{
		reader: reader,
		hardcodedDefaults: map[string]interface{}{
			"RatingMin": int32(1),
			"RatingMax": int32(5),
		},
	}
}

func (a *ReviewConfigAdapter) RatingMin(ctx context.Context) (int32, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/review/rating/min")
	if err == nil {
		if converted, ok := toInt32(val); ok {
			a.lastKnownCache.Store("RatingMin", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("RatingMin"); ok {
		if converted, ok := toInt32(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["RatingMin"].(int32), nil
}

func (a *ReviewConfigAdapter) RatingMax(ctx context.Context) (int32, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/review/rating/max")
	if err == nil {
		if converted, ok := toInt32(val); ok {
			a.lastKnownCache.Store("RatingMax", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("RatingMax"); ok {
		if converted, ok := toInt32(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["RatingMax"].(int32), nil
}
