package httpreader

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
)

func TestNew_DefaultEndpoint(t *testing.T) {
	// 清除环境变量
	os.Unsetenv(EnvKey)

	r := New()
	if r.endpoint != DefaultEndpoint {
		t.Errorf("New() endpoint=%q, want %q", r.endpoint, DefaultEndpoint)
	}
}

func TestNewWithEndpoint_Explicit(t *testing.T) {
	r := NewWithEndpoint("example.com:9090")
	if r.endpoint != "example.com:9090" {
		t.Errorf("NewWithEndpoint() endpoint=%q, want %q", r.endpoint, "example.com:9090")
	}
}

func TestNewWithEndpoint_EnvFallback(t *testing.T) {
	os.Setenv(EnvKey, "myhost:9999")
	defer os.Unsetenv(EnvKey)

	r := NewWithEndpoint("")
	if r.endpoint != "myhost:9999" {
		t.Errorf("NewWithEndpoint(empty) endpoint=%q, want %q", r.endpoint, "myhost:9999")
	}
}

func TestNewWithEndpoint_StripProtocol(t *testing.T) {
	r := NewWithEndpoint("http://10.0.0.1:8080")
	if r.endpoint != "10.0.0.1:8080" {
		t.Errorf("NewWithEndpoint(http://...) endpoint=%q, want %q", r.endpoint, "10.0.0.1:8080")
	}
}

func TestGet_Found(t *testing.T) {
	// 启动 mock Electron HTTP 服务
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/config/biz_auth/auth/jwt/token_expiry_seconds" {
			t.Errorf("请求路径 = %q, 期望 = %q", r.URL.Path, "/config/biz_auth/auth/jwt/token_expiry_seconds")
		}
		resp := ConfigResponse{Value: "3600", Found: true}
		json.NewEncoder(w).Encode(resp)
	}))
	defer ts.Close()

	r := NewWithEndpoint(ts.Listener.Addr().String())
	val, err := r.Get(context.Background(), "/config/biz_auth/auth/jwt/token_expiry_seconds")
	if err != nil {
		t.Fatalf("Get() 失败: %v", err)
	}
	if val != "3600" {
		t.Errorf("Get() = %q, want %q", val, "3600")
	}
}

func TestGet_NotFound(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(ConfigResponse{Value: "", Found: false})
	}))
	defer ts.Close()

	r := NewWithEndpoint(ts.Listener.Addr().String())
	_, err := r.Get(context.Background(), "/config/biz_auth/unknown/key")
	if err == nil {
		t.Fatal("期望 Get() 返回错误，但返回 nil")
	}
}

func TestGet_ServerError(t *testing.T) {
	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusInternalServerError)
		w.Write([]byte("internal error"))
	}))
	defer ts.Close()

	r := NewWithEndpoint(ts.Listener.Addr().String())
	_, err := r.Get(context.Background(), "/config/biz_auth/test/key")
	if err == nil {
		t.Fatal("期望 Get() 返回错误，但返回 nil")
	}
}

func TestGet_ConnectionRefused(t *testing.T) {
	r := NewWithEndpoint("127.0.0.1:1") // 没有服务在监听
	_, err := r.Get(context.Background(), "/config/biz_auth/test/key")
	if err == nil {
		t.Fatal("期望 Get() 返回连接错误，但返回 nil")
	}
}

func TestClose(t *testing.T) {
	r := New()
	r.Close() // 应不 panic
}
