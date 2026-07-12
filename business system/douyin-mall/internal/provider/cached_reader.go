// Package douyin_mall 提供 BlessStar 配置的依赖注入和可选装饰器。
// 请勿手动修改 — 由 blessstar-codegen 自动生成
package douyin_mall

import (
	"context"
	"sync"
	"time"

	"douyin-mall-go-template/ports"
)

// CachedReader 是对 ConfigReader 的缓存包装器（可选装饰器）。
// 后台协程使用 time.Ticker 定时刷新缓存，实现秒级准实时热更新。
// 不依赖任何外部库，仅使用 Go 标准库。
//
// 使用方式（由 main.go 注入）：
//
//	rawReader := httpReader.New("BLESSSTAR_ENDPOINT")
//	cachedReader := provider.NewCachedReader(rawReader, 30*time.Second)
//	adapters := provider.ProvideBlessStarAdapters(cachedReader)
type CachedReader struct {
	inner       ports.ConfigReader
	cache       sync.Map
	ticker      *time.Ticker
	refreshFunc func(ctx context.Context) error
}

// NewCachedReader 创建 CachedReader 实例。
// interval 控制缓存的刷新周期（如 30 秒）。
// 业务方可设置 RefreshFunc 自定义全量刷新逻辑。
func NewCachedReader(inner ports.ConfigReader, interval time.Duration) *CachedReader {
	cr := &CachedReader{
		inner:       inner,
		ticker:      time.NewTicker(interval),
		refreshFunc: func(ctx context.Context) error { return nil }, // 默认空操作
	}
	go cr.refreshLoop(context.Background())
	return cr
}

// RefreshFunc 设置全量刷新回调函数，由业务方自定义需要缓存的配置路径。
func (c *CachedReader) RefreshFunc(fn func(ctx context.Context) error) *CachedReader {
	c.refreshFunc = fn
	return c
}

// Get 从缓存中读取配置值。若缓存命中直接返回，否则穿透到 inner.ConfigReader。
func (c *CachedReader) Get(ctx context.Context, path string) (interface{}, error) {
	if val, ok := c.cache.Load(path); ok {
		return val, nil
	}
	return c.inner.Get(ctx, path)
}

// refreshLoop 后台协程，定时执行全量刷新。
func (c *CachedReader) refreshLoop(ctx context.Context) {
	for range c.ticker.C {
		if err := c.refreshFunc(ctx); err != nil {
			// 刷新失败不影响现有缓存，仅跳过本轮
			continue
		}
	}
}

// Close 停止后台刷新协程。
func (c *CachedReader) Close() {
	c.ticker.Stop()
}
