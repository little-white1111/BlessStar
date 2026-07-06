// Package httpreader 实现 ports.ConfigReader 接口，通过 HTTP 从 BlessStar Editor 读取配置。
//
// 使用方式：
//
//	import "your-project/httpreader"
//
//	reader := httpreader.New()
//	adapters := provider.ProvideBlessStarAdapters(reader)
//
// 环境变量：
//   - BLESSSTAR_ENDPOINT: Electron HTTP 服务地址（默认 "localhost:8080"）
//
// 对应 Electron 端 HTTP API：
//   GET /config/{biz_id}/{path} → {"value": "...", "found": true}
//
// 设计原则：
//   - 零外部依赖，仅使用 Go 标准库
//   - 非阻塞初始化：首次 Get() 才发起 HTTP 请求
//   - 连接失败时返回 error，由 adapter 的三阶段降级兜底
package httpreader

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

// DefaultEndpoint 是 Electron HTTP 服务的默认地址。
const DefaultEndpoint = "localhost:8080"

// EnvKey 是配置 Electron 地址的环境变量名。
const EnvKey = "BLESSSTAR_ENDPOINT"

// ConfigResponse 是 Electron HTTP API 返回的 JSON 格式。
type ConfigResponse struct {
	Value string `json:"value"`
	Found bool   `json:"found"`
}

// HTTPReader 通过 HTTP 从 BlessStar Editor（Electron）读取配置值。
// 实现 ports.ConfigReader 接口。
type HTTPReader struct {
	endpoint   string
	httpClient *http.Client
}

// New 创建 HTTPReader 实例。
// 从环境变量 BLESSSTAR_ENDPOINT 读取 Electron 地址，不存在时使用默认值 localhost:8080。
// 初始化时不发起连接探测，首次 Get() 时才真正发起 HTTP 请求。
func New() *HTTPReader {
	return NewWithEndpoint("")
}

// NewWithEndpoint 使用指定地址创建 HTTPReader 实例。
// 如果 endpoint 为空字符串，回退到环境变量 BLESSSTAR_ENDPOINT；环境变量也为空时使用默认值。
func NewWithEndpoint(endpoint string) *HTTPReader {
	if endpoint == "" {
		endpoint = os.Getenv(EnvKey)
	}
	if endpoint == "" {
		endpoint = DefaultEndpoint
	}
	// 移除协议前缀（如有）
	endpoint = strings.TrimPrefix(endpoint, "http://")
	endpoint = strings.TrimPrefix(endpoint, "https://")

	return &HTTPReader{
		endpoint: endpoint,
		httpClient: &http.Client{
			Timeout: 5 * time.Second,
		},
	}
}

// Get 通过 HTTP 从 Electron 读取配置值。
// path 是配置的完整注册路径（如 "/config/biz_auth/auth/jwt/token_expiry_seconds"），
// 由 adapter 自动生成并传入。
//
// 返回值：
//   - 配置值（string 类型），业务方根据需要在 adapter 中做类型断言或转换
//   - 如果配置不存在或请求失败返回 error，由 adapter 的三阶段降级处理
func (r *HTTPReader) Get(ctx context.Context, path string) (interface{}, error) {
	// 构造 URL
	// path 已含 /config/{biz_id}/ 前缀，例如 /config/biz_auth/auth/jwt/token_expiry_seconds
	cleanPath := strings.TrimPrefix(path, "/")
	url := fmt.Sprintf("http://%s/%s", r.endpoint, cleanPath)

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return "", fmt.Errorf("httpreader: 创建请求失败: %w", err)
	}

	resp, err := r.httpClient.Do(req)
	if err != nil {
		return "", fmt.Errorf("httpreader: 请求失败 (%s): %w", url, err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("httpreader: 读取响应失败: %w", err)
	}

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("httpreader: HTTP %d (%s)", resp.StatusCode, strings.TrimSpace(string(body)))
	}

	var configResp ConfigResponse
	if err := json.Unmarshal(body, &configResp); err != nil {
		return "", fmt.Errorf("httpreader: 解析响应失败: %w", err)
	}

	if !configResp.Found {
		return "", fmt.Errorf("httpreader: 配置未找到: %s", path)
	}

	return configResp.Value, nil
}

// Close 关闭 HTTP 客户端连接池（可选）。
// 使用完毕后调用以释放资源。
func (r *HTTPReader) Close() {
	if r.httpClient != nil {
		r.httpClient.CloseIdleConnections()
	}
}
