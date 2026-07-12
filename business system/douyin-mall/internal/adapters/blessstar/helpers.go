// Package adapter_blessstar 提供 BlessStar 配置适配器的类型安全转换辅助函数。
// 自动生成于 BlessStar 配置端口-适配器 — 请勿手动修改。

package adapter_blessstar

import (
	"fmt"
	"time"
)

// toInt32 安全地将 interface{} 转换为 int32。
// 支持从 int、int32、int64、float64 转换。
func toInt32(val interface{}) (int32, bool) {
	switch v := val.(type) {
	case int32:
		return v, true
	case int:
		return int32(v), true
	case int64:
		return int32(v), true
	case float64:
		return int32(v), true
	default:
		return 0, false
	}
}

// toBool 安全地将 interface{} 转换为 bool。
func toBool(val interface{}) (bool, bool) {
	v, ok := val.(bool)
	return v, ok
}

// toString 安全地将 interface{} 转换为 string。
func toString(val interface{}) (string, bool) {
	switch v := val.(type) {
	case string:
		return v, true
	default:
		return fmt.Sprint(val), false
	}
}

// toStringSlice 安全地将 interface{} 转换为 []string。
// 支持从 []string 和 []interface{} 转换。
func toStringSlice(val interface{}) ([]string, bool) {
	switch v := val.(type) {
	case []string:
		return v, true
	case []interface{}:
		result := make([]string, len(v))
		for i, item := range v {
			result[i] = fmt.Sprint(item)
		}
		return result, true
	default:
		return nil, false
	}
}

// toDurationSeconds 将代表秒数的值转换为 time.Duration。
// config_metadata 中 I64 类型的值以秒为单位存储。
func toDurationSeconds(val interface{}) (time.Duration, bool) {
	switch v := val.(type) {
	case time.Duration:
		return v, true
	case int:
		return time.Duration(v) * time.Second, true
	case int32:
		return time.Duration(v) * time.Second, true
	case int64:
		return time.Duration(v) * time.Second, true
	case float64:
		return time.Duration(v) * time.Second, true
	default:
		return 0, false
	}
}
