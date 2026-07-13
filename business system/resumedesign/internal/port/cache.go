package port

import "time"

// Cache KV 缓存 Port 接口
// 职责：缓存原始简历等不常变更但频繁读取的数据
// 实现：adapter/cache/sqlite.go（基于 SQLite 的 KV 表）
type Cache interface {
	// Get 读取缓存
	Get(key string) (string, error)
	// Set 写入缓存并设置 TTL
	Set(key string, value string, ttl time.Duration) error
	// Delete 删除缓存
	Delete(key string) error
}
