// Package adapter_http 提供基于 HTTP 协议的 BlessStar ConfigReader 实现。
// 通过环境变量 BLESSSTAR_ENDPOINT 获取 Electron 地址，非阻塞启动。
package adapter_http

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"
)

// HTTPReader 通过 HTTP 请求从 BlessStar Electron 进程拉取配置。
// 实现 ports.ConfigReader 接口。
type HTTPReader struct {
	endpoint   string
	bizID      string
	httpClient *http.Client
}

// New 创建 HTTPReader 实例。
// endpoint 从环境变量 BLESSSTAR_ENDPOINT 读取，若为空则返回 nil（走 adapter 三阶段降级）。
// bizID 用于构造请求路径（如 "douyin-mall"）。
func New(bizID string) *HTTPReader {
	endpoint := os.Getenv("BLESSSTAR_ENDPOINT")
	if endpoint == "" {
		return nil // 无配置源时返回 nil，由 adapter 的 Cache→Default 兜底
	}
	// 移除尾部斜杠
	endpoint = strings.TrimRight(endpoint, "/")

	return &HTTPReader{
		endpoint: endpoint,
		bizID:    bizID,
		httpClient: &http.Client{
			Timeout: 5 * time.Second, // 5 秒超时，避免阻塞
		},
	}
}

// Get 从 BlessStar 电子进程获取配置值。
// path 是配置的完整注册路径（如 "/config/douyin-mall/auth/jwt/token_expiry_seconds"）。
// 返回 interface{} 以便 adapter 进行类型断言。
func (r *HTTPReader) Get(ctx context.Context, path string) (interface{}, error) {
	if r == nil {
		return nil, fmt.Errorf("HTTPReader not initialized: BLESSSTAR_ENDPOINT is empty")
	}

	url := fmt.Sprintf("%s%s", r.endpoint, path)

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return nil, fmt.Errorf("create request failed: %w", err)
	}

	resp, err := r.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("unexpected status code: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read body failed: %w", err)
	}

	// Try to parse the response as JSON with a "value" field
	// BlessStar API typically returns {"value": <actual_value>}
	var result map[string]json.RawMessage
	if err := json.Unmarshal(body, &result); err == nil {
		if rawVal, ok := result["value"]; ok {
			// Attempt to parse the value field as various types
			return parseJSONValue(rawVal)
		}
		// If no "value" field, return the raw JSON as string
		return string(body), nil
	}

	// Plain text response
	return strings.TrimSpace(string(body)), nil
}

// parseJSONValue 将 JSON 原始消息解析为 Go 类型。
func parseJSONValue(raw json.RawMessage) (interface{}, error) {
	// Try string first
	var strVal string
	if err := json.Unmarshal(raw, &strVal); err == nil {
		return strVal, nil
	}

	// Try number (int64 for integer, float64 for float)
	var numVal json.Number
	if err := json.Unmarshal(raw, &numVal); err == nil {
		if strings.Contains(numVal.String(), ".") {
			return numVal.Float64()
		}
		return strconv.ParseInt(numVal.String(), 10, 64)
	}

	// Try bool
	var boolVal bool
	if err := json.Unmarshal(raw, &boolVal); err == nil {
		return boolVal, nil
	}

	// Try array
	var arrVal []interface{}
	if err := json.Unmarshal(raw, &arrVal); err == nil {
		return arrVal, nil
	}

	// Try map
	var mapVal map[string]interface{}
	if err := json.Unmarshal(raw, &mapVal); err == nil {
		return mapVal, nil
	}

	// Fallback: return raw bytes as string
	return string(raw), nil
}
