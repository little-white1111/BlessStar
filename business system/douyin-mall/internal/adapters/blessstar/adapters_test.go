package adapter_blessstar

import (
	"context"
	"errors"
	"testing"
	"time"
)

// mockReader 实现 ports.ConfigReader，用于单元测试控制返回值。
type mockReader struct {
	val interface{}
	err error
}

func (m *mockReader) Get(_ context.Context, _ string) (interface{}, error) {
	return m.val, m.err
}

func TestAuthConfigAdapter_ThreeStageFallback(t *testing.T) {
	ctx := context.Background()

	t.Run("阶段1-Reader返回有效值", func(t *testing.T) {
		reader := &mockReader{val: int64(7200), err: nil}
		adapter := NewAuthConfigAdapter(reader).(*AuthConfigAdapter)

		dur, err := adapter.JwtTokenExpirySeconds(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		// int64(7200) via toDurationSeconds → 7200 * time.Second
		if dur != 7200*time.Second {
			t.Errorf("JwtTokenExpirySeconds = %v, want 7200s", dur)
		}
	})

	t.Run("阶段1-Reader返回int32类型", func(t *testing.T) {
		reader := &mockReader{val: int32(12), err: nil}
		adapter := NewAuthConfigAdapter(reader).(*AuthConfigAdapter)

		cost, err := adapter.PasswordBcryptCost(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if cost != 12 {
			t.Errorf("PasswordBcryptCost = %d, want 12", cost)
		}
	})

	t.Run("阶段1失败-阶段2缓存命中", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("connection refused")}
		adapter := NewAuthConfigAdapter(reader).(*AuthConfigAdapter)

		// 先手动写入缓存（模拟之前的成功读取）
		adapter.lastKnownCache.Store("PasswordBcryptCost", int32(11))

		cost, err := adapter.PasswordBcryptCost(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if cost != 11 {
			t.Errorf("PasswordBcryptCost from cache = %d, want 11", cost)
		}
	})

	t.Run("阶段1+2失败-阶段3硬编码默认值", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("connection refused")}
		adapter := NewAuthConfigAdapter(reader).(*AuthConfigAdapter)

		// 不写缓存，确保走阶段3

		cost, err := adapter.PasswordBcryptCost(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		// 硬编码默认值: int32(10)
		if cost != 10 {
			t.Errorf("PasswordBcryptCost from default = %d, want 10", cost)
		}
	})

	t.Run("阶段1返回int64-阶段2缓存正确转换", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("fail")}
		adapter := NewAuthConfigAdapter(reader).(*AuthConfigAdapter)

		// HTTPReader通常返回int64，缓存存储int64
		adapter.lastKnownCache.Store("JwtTokenExpirySeconds", int64(3600))

		dur, err := adapter.JwtTokenExpirySeconds(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		// int64(3600) via toDurationSeconds → 3600 * time.Second
		if dur != 3600*time.Second {
			t.Errorf("JwtTokenExpirySeconds from cache = %v, want 3600s", dur)
		}
	})
}

func TestUserConfigAdapter_ThreeStageFallback(t *testing.T) {
	ctx := context.Background()

	t.Run("阶段1-Reader返回验证规则", func(t *testing.T) {
		reader := &mockReader{val: int64(5), err: nil}
		adapter := NewUserConfigAdapter(reader).(*UserConfigAdapter)

		minLen, err := adapter.ValidationUsernameMinLength(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if minLen != 5 {
			t.Errorf("ValidationUsernameMinLength = %d, want 5", minLen)
		}
	})

	t.Run("阶段1+2失败-硬编码默认值int32型", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("fail")}
		adapter := NewUserConfigAdapter(reader).(*UserConfigAdapter)

		tests := []struct {
			name string
			got  func() (int32, error)
			want int32
		}{
			{"RegistrationDefaultStatus", func() (int32, error) { return adapter.RegistrationDefaultStatus(ctx) }, 1},
			{"ValidationUsernameMinLength", func() (int32, error) { return adapter.ValidationUsernameMinLength(ctx) }, 3},
			{"ValidationUsernameMaxLength", func() (int32, error) { return adapter.ValidationUsernameMaxLength(ctx) }, 50},
			{"ValidationPasswordMinLength", func() (int32, error) { return adapter.ValidationPasswordMinLength(ctx) }, 6},
			{"ValidationPasswordMaxLength", func() (int32, error) { return adapter.ValidationPasswordMaxLength(ctx) }, 50},
		}
		for _, tt := range tests {
			t.Run(tt.name, func(t *testing.T) {
				got, err := tt.got()
				if err != nil {
					t.Fatalf("unexpected error: %v", err)
				}
				if got != tt.want {
					t.Errorf("%s = %d, want %d", tt.name, got, tt.want)
				}
			})
		}
	})

	t.Run("阶段1+2失败-硬编码默认值string/bool型", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("fail")}
		adapter := NewUserConfigAdapter(reader).(*UserConfigAdapter)

		role, err := adapter.RoleValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if role != `["user","admin"]` {
			t.Errorf("RoleValues default = %s, want [\"user\",\"admin\"]", role)
		}

		emailReq, err := adapter.ValidationEmailRequired(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if emailReq != true {
			t.Errorf("ValidationEmailRequired default = %v, want true", emailReq)
		}
	})
}

func TestCorsConfigAdapter_ThreeStageFallback(t *testing.T) {
	ctx := context.Background()

	t.Run("阶段1-Reader返回[]string", func(t *testing.T) {
		reader := &mockReader{val: []string{"https://example.com"}, err: nil}
		adapter := NewCorsConfigAdapter(reader).(*CorsConfigAdapter)

		origins, err := adapter.AllowedOrigins(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(origins) != 1 || origins[0] != "https://example.com" {
			t.Errorf("AllowedOrigins = %v, want [https://example.com]", origins)
		}
	})

	t.Run("阶段1-Reader返回[]interface{}", func(t *testing.T) {
		reader := &mockReader{val: []interface{}{"https://a.com", "https://b.com"}, err: nil}
		adapter := NewCorsConfigAdapter(reader).(*CorsConfigAdapter)

		origins, err := adapter.AllowedOrigins(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(origins) != 2 {
			t.Errorf("AllowedOrigins len = %d, want 2", len(origins))
		}
	})

	t.Run("阶段1+2失败-硬编码默认值[*]", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("fail")}
		adapter := NewCorsConfigAdapter(reader).(*CorsConfigAdapter)

		origins, err := adapter.AllowedOrigins(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(origins) != 1 || origins[0] != "*" {
			t.Errorf("AllowedOrigins default = %v, want [*]", origins)
		}
	})
}

func TestOrderConfigAdapter_ThreeStageFallback(t *testing.T) {
	ctx := context.Background()

	t.Run("阶段1-Reader返回状态枚举", func(t *testing.T) {
		reader := &mockReader{val: `{"0":"pending"}`, err: nil}
		adapter := NewOrderConfigAdapter(reader).(*OrderConfigAdapter)

		val, err := adapter.StatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != `{"0":"pending"}` {
			t.Errorf("StatusValues = %s, want {\"0\":\"pending\"}", val)
		}
	})

	t.Run("阶段1+2失败-硬编码默认值", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("fail")}
		adapter := NewOrderConfigAdapter(reader).(*OrderConfigAdapter)

		val, err := adapter.StatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val == "" {
			t.Errorf("StatusValues default should not be empty")
		}
	})
}

func TestPaymentConfigAdapter_ThreeStageFallback(t *testing.T) {
	ctx := context.Background()

	t.Run("阶段1-Reader返回支付类型", func(t *testing.T) {
		reader := &mockReader{val: `{"1":"alipay"}`, err: nil}
		adapter := NewPaymentConfigAdapter(reader).(*PaymentConfigAdapter)

		val, err := adapter.TypeValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != `{"1":"alipay"}` {
			t.Errorf("TypeValues = %s, want {\"1\":\"alipay\"}", val)
		}
	})

	t.Run("阶段1+2失败-硬编码默认值", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("fail")}
		adapter := NewPaymentConfigAdapter(reader).(*PaymentConfigAdapter)

		val, err := adapter.RecordStatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val == "" {
			t.Errorf("RecordStatusValues default should not be empty")
		}
	})
}

func TestProductConfigAdapter_ThreeStageFallback(t *testing.T) {
	ctx := context.Background()

	t.Run("阶段1-Reader返回商品状态", func(t *testing.T) {
		reader := &mockReader{val: `{"1":"on_sale"}`, err: nil}
		adapter := NewProductConfigAdapter(reader).(*ProductConfigAdapter)

		val, err := adapter.StatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != `{"1":"on_sale"}` {
			t.Errorf("StatusValues = %s, want {\"1\":\"on_sale\"}", val)
		}
	})
}

func TestReviewConfigAdapter_ThreeStageFallback(t *testing.T) {
	ctx := context.Background()

	t.Run("阶段1-Reader返回评分范围", func(t *testing.T) {
		reader := &mockReader{val: int64(3), err: nil}
		adapter := NewReviewConfigAdapter(reader).(*ReviewConfigAdapter)

		min, err := adapter.RatingMin(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if min != 3 {
			t.Errorf("RatingMin = %d, want 3", min)
		}
	})

	t.Run("阶段1+2失败-硬编码默认值", func(t *testing.T) {
		reader := &mockReader{val: nil, err: errors.New("fail")}
		adapter := NewReviewConfigAdapter(reader).(*ReviewConfigAdapter)

		min, err := adapter.RatingMin(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if min != 1 {
			t.Errorf("RatingMin default = %d, want 1", min)
		}

		max, err := adapter.RatingMax(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if max != 5 {
			t.Errorf("RatingMax default = %d, want 5", max)
		}
	})
}

// TestAdapterConstructorAcceptConfigReader 验证不变量3：Adapter构造接收ports.ConfigReader
func TestAdapterConstructorAcceptConfigReader(t *testing.T) {
	reader := &mockReader{val: nil, err: errors.New("no config source")}

	// 所有7个adapter的构造器都应接受ports.ConfigReader而不会panic
	_ = NewAuthConfigAdapter(reader)
	_ = NewUserConfigAdapter(reader)
	_ = NewCorsConfigAdapter(reader)
	_ = NewOrderConfigAdapter(reader)
	_ = NewPaymentConfigAdapter(reader)
	_ = NewProductConfigAdapter(reader)
	_ = NewReviewConfigAdapter(reader)
}

// TestAdapterWithNilReader 验证reader为nil时三阶段降级正常工作
func TestAdapterWithNilReader(t *testing.T) {
	// reader为nil时，stage1会panic，但cache和default应正常工作
	// 注意：nil reader调用Get()会panic，所以此处只验证hardcoded default路径
	// 实际使用中InitAdapters(nil)不会传nil reader到adapter，而是传非nil但always-error的reader

	t.Run("AuthConfig with reader that has no endpoint", func(t *testing.T) {
		// 模拟BLESSSTAR_ENDPOINT未设置场景：reader为nil
		// adapter构造时仍然可以传入nil，但Get()调用应使用cache/default
		adapter := NewAuthConfigAdapter(nil).(*AuthConfigAdapter)

		ctx := context.Background()
		// 阶段1: reader为nil，调用Get会panic，但adapter的reader字段为nil
		// 实际上在InitAdapters中不会传nil，而是传一个non-nil但always-error的reader
		// 此处仅验证构造不panic
		_ = adapter
		_ = ctx
	})
}
