package douyin_mall

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"douyin-mall-go-template/ports"
)

// spyReader 记录 Get 调用的次数和参数。
type spyReader struct {
	mu          sync.Mutex
	callCount   int
	lastPath    string
	returnVal   interface{}
	returnErr   error
}

func (s *spyReader) Get(ctx context.Context, path string) (interface{}, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.callCount++
	s.lastPath = path
	return s.returnVal, s.returnErr
}

func (s *spyReader) CallCount() int {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.callCount
}

func TestCachedReader_Get(t *testing.T) {
	ctx := context.Background()

	t.Run("缓存命中-不穿透inner", func(t *testing.T) {
		spy := &spyReader{returnVal: "original", returnErr: nil}
		cr := NewCachedReader(spy, 10*time.Minute) // long interval to avoid Ticker during test

		// 手动预热缓存
		cr.cache.Store("/test/key", "cached_value")

		val, err := cr.Get(ctx, "/test/key")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != "cached_value" {
			t.Errorf("Get() = %v, want cached_value", val)
		}
		if spy.CallCount() != 0 {
			t.Errorf("inner.Get should not be called on cache hit, called %d times", spy.CallCount())
		}
	})

	t.Run("缓存未命中-穿透内层reader", func(t *testing.T) {
		spy := &spyReader{returnVal: "fresh_value", returnErr: nil}
		cr := NewCachedReader(spy, 10*time.Minute)

		val, err := cr.Get(ctx, "/test/miss")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != "fresh_value" {
			t.Errorf("Get() = %v, want fresh_value", val)
		}
		if spy.CallCount() != 1 {
			t.Errorf("inner.Get should be called once on cache miss, called %d times", spy.CallCount())
		}
		if spy.lastPath != "/test/miss" {
			t.Errorf("inner.Get path = %s, want /test/miss", spy.lastPath)
		}
	})

	t.Run("缓存未命中+inner返回error", func(t *testing.T) {
		spy := &spyReader{returnVal: nil, returnErr: errors.New("timeout")}
		cr := NewCachedReader(spy, 10*time.Minute)

		val, err := cr.Get(ctx, "/test/error")
		if err == nil {
			t.Fatal("expected error from inner.Get, got nil")
		}
		if val != nil {
			t.Errorf("Get() = %v, want nil on error", val)
		}
	})

	t.Run("缓存命中不同key互不干扰", func(t *testing.T) {
		spy := &spyReader{returnVal: "default", returnErr: nil}
		cr := NewCachedReader(spy, 10*time.Minute)

		cr.cache.Store("/key/a", "value_a")
		cr.cache.Store("/key/b", "value_b")

		valA, _ := cr.Get(ctx, "/key/a")
		valB, _ := cr.Get(ctx, "/key/b")
		if valA != "value_a" || valB != "value_b" {
			t.Errorf("cache keys interfere: a=%v, b=%v", valA, valB)
		}
	})
}

func TestCachedReader_Close(t *testing.T) {
	cr := NewCachedReader(&spyReader{}, 10*time.Millisecond)

	// Close 应该停止 Ticker
	cr.Close()

	// 等待一段时间确认没有 panic（Ticker 已停止）
	time.Sleep(50 * time.Millisecond)
}

func TestCachedReader_RefreshFunc(t *testing.T) {
	ctx := context.Background()
	spy := &spyReader{returnVal: "refreshed", returnErr: nil}
	cr := NewCachedReader(spy, 10*time.Minute)

	refreshCalled := false
	cr.RefreshFunc(func(ctx context.Context) error {
		refreshCalled = true
		// 模拟全量刷新：写缓存
		cr.cache.Store("/refresh/key", "post_refresh")
		return nil
	})

	// 手动触发 refreshLoop（通过调用 refreshFunc 间接模拟）
	// 实际 refreshLoop 由 Ticker 驱动，测试中手动验证回调设置
	cr.refreshFunc(ctx)

	if !refreshCalled {
		t.Error("RefreshFunc was not called via refreshFunc field")
	}

	// 验证刷新后的值
	val, err := cr.Get(ctx, "/refresh/key")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if val != "post_refresh" {
		t.Errorf("after refresh Get() = %v, want post_refresh", val)
	}
}

// TestCachedReader_ImplementsConfigReader 验证 CachedReader 实现 ports.ConfigReader
func TestCachedReader_ImplementsConfigReader(t *testing.T) {
	var cr ports.ConfigReader = NewCachedReader(&spyReader{}, 10*time.Minute)
	_ = cr // 编译时验证接口实现
}
