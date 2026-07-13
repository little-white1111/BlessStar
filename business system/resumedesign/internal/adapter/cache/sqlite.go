package cache

import (
	"crypto/sha256"
	"fmt"
	"sync"
	"time"
)

// cacheEntry 缓存条目
type cacheEntry struct {
	Value      string
	ExpiresAt  time.Time
}

// SQLiteCache 基于内存的 KV 缓存（简化实现）
// 设计上占位 adapter/cache/，后续可切换到基于 SQLite 的持久化 KV 存储
type SQLiteCache struct {
	mu    sync.RWMutex
	store map[string]cacheEntry
}

func NewSQLiteCache() *SQLiteCache {
	return &SQLiteCache{
		store: make(map[string]cacheEntry),
	}
}

func (c *SQLiteCache) Get(key string) (string, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	entry, ok := c.store[key]
	if !ok {
		return "", fmt.Errorf("cache miss: %s", key)
	}

	if time.Now().After(entry.ExpiresAt) {
		delete(c.store, key)
		return "", fmt.Errorf("cache expired: %s", key)
	}

	return entry.Value, nil
}

func (c *SQLiteCache) Set(key string, value string, ttl time.Duration) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.store[key] = cacheEntry{
		Value:     value,
		ExpiresAt: time.Now().Add(ttl),
	}
	return nil
}

func (c *SQLiteCache) Delete(key string) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	delete(c.store, key)
	return nil
}

// Key 生成缓存键（基于原始简历内容哈希）
func CacheKey(content string) string {
	h := sha256.Sum256([]byte(content))
	return fmt.Sprintf("resume_cache:%x", h[:8])
}
