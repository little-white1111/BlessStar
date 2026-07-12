package adapter_http

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"
)

// TestNew_EmptyEndpoint 验证 BLESSSTAR_ENDPOINT 未设置时返回 nil
func TestNew_EmptyEndpoint(t *testing.T) {
	// 确保环境变量未设置
	os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	if reader != nil {
		t.Errorf("New() with empty BLESSSTAR_ENDPOINT should return nil, got %v", reader)
	}
}

// TestNew_WithEndpoint 验证设置了端点时正确构造
func TestNew_WithEndpoint(t *testing.T) {
	os.Setenv("BLESSSTAR_ENDPOINT", "http://localhost:9999")
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	if reader == nil {
		t.Fatal("New() with valid endpoint should not return nil")
	}
	if reader.endpoint != "http://localhost:9999" {
		t.Errorf("endpoint = %s, want http://localhost:9999", reader.endpoint)
	}
	if reader.bizID != "test-biz" {
		t.Errorf("bizID = %s, want test-biz", reader.bizID)
	}
}

// TestGet_JSONResponse 验证 JSON 格式响应解析
func TestGet_JSONResponse(t *testing.T) {
	// 创建一个返回 JSON 的测试服务器
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/config/test-biz/auth/jwt/expiry" {
			t.Errorf("unexpected path: %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"value": 86400,
			"found": true,
		})
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	if reader == nil {
		t.Fatal("New() should succeed")
	}

	ctx := context.Background()
	val, err := reader.Get(ctx, "/config/test-biz/auth/jwt/expiry")
	if err != nil {
		t.Fatalf("Get() failed: %v", err)
	}

	// JSON 数字应被解析为 int64
	intVal, ok := val.(int64)
	if !ok {
		t.Fatalf("value type = %T, want int64", val)
	}
	if intVal != 86400 {
		t.Errorf("value = %d, want 86400", intVal)
	}
}

// TestGet_JSONStringValue 验证 JSON 字符串值解析
func TestGet_JSONStringValue(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewEncoder(w).Encode(map[string]string{
			"value": "user",
		})
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	ctx := context.Background()
	val, err := reader.Get(ctx, "/config/test-biz/user/role")
	if err != nil {
		t.Fatalf("Get() failed: %v", err)
	}

	strVal, ok := val.(string)
	if !ok {
		t.Fatalf("value type = %T, want string", val)
	}
	if strVal != "user" {
		t.Errorf("value = %s, want user", strVal)
	}
}

// TestGet_JSONArrayValue 验证 JSON 数组值解析
func TestGet_JSONArrayValue(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewEncoder(w).Encode(map[string]interface{}{
			"value": []interface{}{"https://a.com", "https://b.com"},
		})
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	ctx := context.Background()
	val, err := reader.Get(ctx, "/config/test-biz/cors/origins")
	if err != nil {
		t.Fatalf("Get() failed: %v", err)
	}

	arrVal, ok := val.([]interface{})
	if !ok {
		t.Fatalf("value type = %T, want []interface{}", val)
	}
	if len(arrVal) != 2 {
		t.Errorf("array len = %d, want 2", len(arrVal))
	}
}

// TestGet_PlainTextResponse 验证纯文本响应
func TestGet_PlainTextResponse(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain")
		w.Write([]byte("plain_text_value"))
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	ctx := context.Background()
	val, err := reader.Get(ctx, "/config/test-biz/some/key")
	if err != nil {
		t.Fatalf("Get() failed: %v", err)
	}

	strVal, ok := val.(string)
	if !ok {
		t.Fatalf("value type = %T, want string", val)
	}
	if strVal != "plain_text_value" {
		t.Errorf("value = %s, want plain_text_value", strVal)
	}
}

// TestGet_HTTPError 验证 HTTP 错误状态码
func TestGet_HTTPError(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusInternalServerError)
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	ctx := context.Background()
	_, err := reader.Get(ctx, "/config/test-biz/error")
	if err == nil {
		t.Fatal("expected error for HTTP 500, got nil")
	}
}

// TestGet_Timeout 验证超时返回 error
func TestGet_Timeout(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		time.Sleep(100 * time.Millisecond) // 模拟慢响应
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	if reader != nil {
		// 使用极短超时
		reader.httpClient.Timeout = 1 * time.Millisecond

		ctx := context.Background()
		_, err := reader.Get(ctx, "/config/test-biz/timeout")
		if err == nil {
			t.Fatal("expected timeout error, got nil")
		}
	}
}

// TestGet_WithContextCancel 验证 ctx 取消传播
func TestGet_WithContextCancel(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		time.Sleep(100 * time.Millisecond)
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	if reader != nil {
		ctx, cancel := context.WithCancel(context.Background())
		cancel() // 立即取消

		_, err := reader.Get(ctx, "/config/test-biz/cancel")
		if err == nil {
			t.Fatal("expected error due to canceled context, got nil")
		}
	}
}

// TestGet_JSONBoolValue 验证 JSON bool 值解析
func TestGet_JSONBoolValue(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		json.NewEncoder(w).Encode(map[string]interface{}{
			"value": true,
		})
	}))
	defer ts.Close()

	os.Setenv("BLESSSTAR_ENDPOINT", ts.URL)
	defer os.Unsetenv("BLESSSTAR_ENDPOINT")

	reader := New("test-biz")
	ctx := context.Background()
	val, err := reader.Get(ctx, "/config/test-biz/flag")
	if err != nil {
		t.Fatalf("Get() failed: %v", err)
	}

	boolVal, ok := val.(bool)
	if !ok {
		t.Fatalf("value type = %T, want bool", val)
	}
	if boolVal != true {
		t.Errorf("value = %v, want true", boolVal)
	}
}
