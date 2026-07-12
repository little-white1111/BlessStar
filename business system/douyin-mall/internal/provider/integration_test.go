package douyin_mall

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	adapter_http "douyin-mall-go-template/internal/adapter_http"
)

// startMockBlessStarServer 返回一个模拟 BlessStar Electron HTTP API 的测试服务器。
// 所有 18 个配置项均按照 config_metadata.json 的 registry_path 和 default_value 返回。
func startMockBlessStarServer(t *testing.T) *httptest.Server {
	t.Helper()

	// 配置值映射: registry_path → JSON value
	configValues := map[string]interface{}{
		"/config/douyin-mall/auth/jwt/token_expiry_seconds":     86400,
		"/config/douyin-mall/auth/password/bcrypt_cost":         10,
		"/config/douyin-mall/user/role/values":                  []interface{}{"user", "admin"},
		"/config/douyin-mall/user/status/values":                map[string]interface{}{"1": "active", "0": "inactive", "-1": "deleted"},
		"/config/douyin-mall/user/registration/default_role":    "user",
		"/config/douyin-mall/user/registration/default_status":  1,
		"/config/douyin-mall/user/validation/username_min_length": 3,
		"/config/douyin-mall/user/validation/username_max_length": 50,
		"/config/douyin-mall/user/validation/password_min_length": 6,
		"/config/douyin-mall/user/validation/password_max_length": 50,
		"/config/douyin-mall/user/validation/email_required":    true,
		"/config/douyin-mall/cors/allowed_origins":              []interface{}{"*"},
		"/config/douyin-mall/order/status/values":               map[string]interface{}{"0": "pending_payment", "1": "paid", "2": "shipped", "3": "delivered", "4": "completed", "-1": "cancelled"},
		"/config/douyin-mall/payment/type/values":               map[string]interface{}{"1": "alipay", "2": "wechat", "3": "credit_card"},
		"/config/douyin-mall/payment/record_status/values":      map[string]interface{}{"0": "pending", "1": "success", "2": "failed", "3": "refunded"},
		"/config/douyin-mall/product/status/values":             map[string]interface{}{"1": "on_sale", "0": "off_sale", "-1": "deleted"},
		"/config/douyin-mall/review/rating/min":                 1,
		"/config/douyin-mall/review/rating/max":                 5,
	}

	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		path := r.URL.Path
		val, ok := configValues[path]
		if !ok {
			http.Error(w, fmt.Sprintf("unknown path: %s", path), http.StatusNotFound)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"value": val,
			"found": true,
		})
	}))

	return ts
}

// TestFullChain_WithBlessStarServer 端到端集成测试：
// 模拟 BlessStar Electron HTTP API → HTTPReader → CachedReader → InitAdapters → 各 Port 接口调用
func TestFullChain_WithBlessStarServer(t *testing.T) {
	// 启动模拟服务器
	ts := startMockBlessStarServer(t)
	defer ts.Close()

	// 设置环境变量
	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	// 创建 HTTPReader
	reader := adapter_http.New("douyin-mall")
	if reader == nil {
		t.Fatal("HTTPReader.New() returned nil with BLESSSTAR_ENDPOINT set")
	}

	// 包装为 CachedReader（5s 刷新间隔，测试环境不依赖 Ticker）
	cachedReader := NewCachedReader(reader, 5*time.Second)
	defer cachedReader.Close()

	// 初始化 DefaultAdapters
	InitAdapters(cachedReader)
	if DefaultAdapters == nil {
		t.Fatal("DefaultAdapters is nil after InitAdapters")
	}

	ctx := context.Background()

	// === 认证鉴权 ===
	t.Run("AuthConfig.JwtTokenExpirySeconds", func(t *testing.T) {
		val, err := DefaultAdapters.AuthConfig.JwtTokenExpirySeconds(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		// 86400 seconds = 24h
		if val != 86400*time.Second {
			t.Errorf("got %v, want 86400s (24h)", val)
		}
	})

	t.Run("AuthConfig.PasswordBcryptCost", func(t *testing.T) {
		val, err := DefaultAdapters.AuthConfig.PasswordBcryptCost(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 10 {
			t.Errorf("got %d, want 10", val)
		}
	})

	// === 用户管理 ===
	t.Run("UserConfig.RoleValues", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.RoleValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != `["user","admin"]` {
			t.Errorf("got %s, expect json array of roles", val)
		}
	})

	t.Run("UserConfig.StatusValues", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.StatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val == "" {
			t.Errorf("StatusValues should not be empty")
		}
	})

	t.Run("UserConfig.RegistrationDefaultRole", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.RegistrationDefaultRole(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != "user" {
			t.Errorf("got %s, want user", val)
		}
	})

	t.Run("UserConfig.RegistrationDefaultStatus", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.RegistrationDefaultStatus(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 1 {
			t.Errorf("got %d, want 1", val)
		}
	})

	t.Run("UserConfig.ValidationUsernameMinLength", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.ValidationUsernameMinLength(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 3 {
			t.Errorf("got %d, want 3", val)
		}
	})

	t.Run("UserConfig.ValidationUsernameMaxLength", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.ValidationUsernameMaxLength(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 50 {
			t.Errorf("got %d, want 50", val)
		}
	})

	t.Run("UserConfig.ValidationPasswordMinLength", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.ValidationPasswordMinLength(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 6 {
			t.Errorf("got %d, want 6", val)
		}
	})

	t.Run("UserConfig.ValidationPasswordMaxLength", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.ValidationPasswordMaxLength(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 50 {
			t.Errorf("got %d, want 50", val)
		}
	})

	t.Run("UserConfig.ValidationEmailRequired", func(t *testing.T) {
		val, err := DefaultAdapters.UserConfig.ValidationEmailRequired(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != true {
			t.Errorf("got %v, want true", val)
		}
	})

	// === 安全策略 ===
	t.Run("CorsConfig.AllowedOrigins", func(t *testing.T) {
		val, err := DefaultAdapters.CorsConfig.AllowedOrigins(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(val) != 1 || val[0] != "*" {
			t.Errorf("got %v, want [*]", val)
		}
	})

	// === 订单管理 ===
	t.Run("OrderConfig.StatusValues", func(t *testing.T) {
		val, err := DefaultAdapters.OrderConfig.StatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val == "" {
			t.Errorf("StatusValues should not be empty")
		}
	})

	// === 支付管理 ===
	t.Run("PaymentConfig.TypeValues", func(t *testing.T) {
		val, err := DefaultAdapters.PaymentConfig.TypeValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val == "" {
			t.Errorf("TypeValues should not be empty")
		}
	})

	t.Run("PaymentConfig.RecordStatusValues", func(t *testing.T) {
		val, err := DefaultAdapters.PaymentConfig.RecordStatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val == "" {
			t.Errorf("RecordStatusValues should not be empty")
		}
	})

	// === 商品管理 ===
	t.Run("ProductConfig.StatusValues", func(t *testing.T) {
		val, err := DefaultAdapters.ProductConfig.StatusValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val == "" {
			t.Errorf("StatusValues should not be empty")
		}
	})

	// === 评价管理 ===
	t.Run("ReviewConfig.RatingMin", func(t *testing.T) {
		val, err := DefaultAdapters.ReviewConfig.RatingMin(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 1 {
			t.Errorf("got %d, want 1", val)
		}
	})

	t.Run("ReviewConfig.RatingMax", func(t *testing.T) {
		val, err := DefaultAdapters.ReviewConfig.RatingMax(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if val != 5 {
			t.Errorf("got %d, want 5", val)
		}
	})
}

// TestFullChain_WithNilReader 验证无 BLESSSTAR_ENDPOINT 时全部走三阶段降级
func TestFullChain_WithNilReader(t *testing.T) {
	// 确保环境变量未设置
	os.Unsetenv("BLESSSTAR_ENDPOINT")

	// InitAdapters(nil) — 所有 adapter 走硬编码默认值
	InitAdapters(nil)

	if DefaultAdapters == nil {
		t.Fatal("DefaultAdapters is nil after InitAdapters(nil)")
	}

	ctx := context.Background()

	// 验证所有 7 个域都能正常返回默认值（不 panic）
	t.Run("AuthConfig default values", func(t *testing.T) {
		dur, err := DefaultAdapters.AuthConfig.JwtTokenExpirySeconds(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if dur != 86400*time.Second {
			t.Errorf("got %v, want 86400s", dur)
		}

		cost, err := DefaultAdapters.AuthConfig.PasswordBcryptCost(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if cost != 10 {
			t.Errorf("got %d, want 10", cost)
		}
	})

	t.Run("UserConfig default values", func(t *testing.T) {
		// string 类型
		role, err := DefaultAdapters.UserConfig.RoleValues(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if role == "" {
			t.Errorf("RoleValues should not be empty")
		}

		// int32 类型
		minLen, err := DefaultAdapters.UserConfig.ValidationUsernameMinLength(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if minLen != 3 {
			t.Errorf("got %d, want 3", minLen)
		}

		// bool 类型
		emailReq, err := DefaultAdapters.UserConfig.ValidationEmailRequired(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if emailReq != true {
			t.Errorf("got %v, want true", emailReq)
		}
	})

	t.Run("CorsConfig default values", func(t *testing.T) {
		origins, err := DefaultAdapters.CorsConfig.AllowedOrigins(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if len(origins) != 1 || origins[0] != "*" {
			t.Errorf("got %v, want [*]", origins)
		}
	})

	t.Run("OrderConfig/PaymentConfig/ProductConfig default values", func(t *testing.T) {
		// string/ENUM 类型 — 验证不为空
		orderStatus, err := DefaultAdapters.OrderConfig.StatusValues(ctx)
		if err != nil || orderStatus == "" {
			t.Errorf("OrderConfig.StatusValues failed: err=%v, val=%s", err, orderStatus)
		}

		paymentType, err := DefaultAdapters.PaymentConfig.TypeValues(ctx)
		if err != nil || paymentType == "" {
			t.Errorf("PaymentConfig.TypeValues failed: err=%v, val=%s", err, paymentType)
		}

		productStatus, err := DefaultAdapters.ProductConfig.StatusValues(ctx)
		if err != nil || productStatus == "" {
			t.Errorf("ProductConfig.StatusValues failed: err=%v, val=%s", err, productStatus)
		}
	})

	t.Run("ReviewConfig default values", func(t *testing.T) {
		min, err := DefaultAdapters.ReviewConfig.RatingMin(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if min != 1 {
			t.Errorf("got %d, want 1", min)
		}

		max, err := DefaultAdapters.ReviewConfig.RatingMax(ctx)
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
		if max != 5 {
			t.Errorf("got %d, want 5", max)
		}
	})
}
