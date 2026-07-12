package douyin_mall

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"sort"
	"testing"
)

// registryEntry 对应 config_metadata.json 中的一条配置记录。
type registryEntry struct {
	ConfigKey    string `json:"config_key"`
	RegistryPath string `json:"registry_path"`
	DataType     string `json:"data_type"`
	DefaultValue string `json:"default_value"`
	BusinessDomain string `json:"business_domain"`
}

// adapterDefaultEntry 记录 adapter 中每个配置的期望默认值和路径。
type adapterDefaultEntry struct {
	configKey    string
	registryPath string
	dataType     string
	defaultValue string
	checkFn      func(ctx context.Context) error // 运行时验证函数
}

// loadRegistry 读取 config_metadata.json 文件。
func loadRegistry(t *testing.T) []registryEntry {
	t.Helper()

	// 从 test 文件所在目录向上查找 biz-registry 目录
	// 测试运行时 cwd 可能是项目根目录或包目录，尝试多种路径
	candidates := []string{
		"../../../../biz-registry/douyin-mall/config_metadata.json",
		"../../../biz-registry/douyin-mall/config_metadata.json",
		// 也尝试当前目录（从项目根运行）
		"biz-registry/douyin-mall/config_metadata.json",
	}

	var data []byte
	var foundPath string
	for _, candidate := range candidates {
		absPath, _ := filepath.Abs(candidate)
		if _, err := os.Stat(absPath); err == nil {
			data, err = os.ReadFile(absPath)
			if err == nil {
				foundPath = absPath
				break
			}
		}
	}

	if data == nil {
		t.Skipf("config_metadata.json not found (tried %v), skip registry alignment test", candidates)
		return nil
	}

	var entries []registryEntry
	if err := json.Unmarshal(data, &entries); err != nil {
		t.Fatalf("failed to parse %s: %v", foundPath, err)
	}

	t.Logf("loaded %d registry entries from %s", len(entries), foundPath)
	return entries
}

// buildAdapterDefaults 构建 adapter 中所有配置的期望映射表。
// 这些值必须与 adapter 代码中的 hardcodedDefaults 和 registry_path 一致。
func buildAdapterDefaults() []adapterDefaultEntry {
	return []adapterDefaultEntry{
		// 认证鉴权
		{configKey: "auth.jwt.token_expiry_seconds", registryPath: "/config/douyin-mall/auth/jwt/token_expiry_seconds", dataType: "I64", defaultValue: "86400"},
		{configKey: "auth.password.bcrypt_cost", registryPath: "/config/douyin-mall/auth/password/bcrypt_cost", dataType: "I32", defaultValue: "10"},
		// 安全策略
		{configKey: "cors.allowed_origins", registryPath: "/config/douyin-mall/cors/allowed_origins", dataType: "ARR", defaultValue: `["*"]`},
		// 订单管理
		{configKey: "order.status.values", registryPath: "/config/douyin-mall/order/status/values", dataType: "ENUM", defaultValue: `{"0":"pending_payment","1":"paid","2":"shipped","3":"delivered","4":"completed","-1":"cancelled"}`},
		// 支付管理
		{configKey: "payment.record_status.values", registryPath: "/config/douyin-mall/payment/record_status/values", dataType: "ENUM", defaultValue: `{"0":"pending","1":"success","2":"failed","3":"refunded"}`},
		{configKey: "payment.type.values", registryPath: "/config/douyin-mall/payment/type/values", dataType: "ENUM", defaultValue: `{"1":"alipay","2":"wechat","3":"credit_card"}`},
		// 商品管理
		{configKey: "product.status.values", registryPath: "/config/douyin-mall/product/status/values", dataType: "ENUM", defaultValue: `{"1":"on_sale","0":"off_sale","-1":"deleted"}`},
		// 评价管理
		{configKey: "review.rating.max", registryPath: "/config/douyin-mall/review/rating/max", dataType: "I32", defaultValue: "5"},
		{configKey: "review.rating.min", registryPath: "/config/douyin-mall/review/rating/min", dataType: "I32", defaultValue: "1"},
		// 用户管理
		{configKey: "user.registration.default_role", registryPath: "/config/douyin-mall/user/registration/default_role", dataType: "STR", defaultValue: "user"},
		{configKey: "user.registration.default_status", registryPath: "/config/douyin-mall/user/registration/default_status", dataType: "I32", defaultValue: "1"},
		{configKey: "user.role.values", registryPath: "/config/douyin-mall/user/role/values", dataType: "ENUM", defaultValue: `["user","admin"]`},
		{configKey: "user.status.values", registryPath: "/config/douyin-mall/user/status/values", dataType: "ENUM", defaultValue: `{"1":"active","0":"inactive","-1":"deleted"}`},
		{configKey: "user.validation.email_required", registryPath: "/config/douyin-mall/user/validation/email_required", dataType: "BOOL", defaultValue: "true"},
		{configKey: "user.validation.password_max_length", registryPath: "/config/douyin-mall/user/validation/password_max_length", dataType: "I32", defaultValue: "50"},
		{configKey: "user.validation.password_min_length", registryPath: "/config/douyin-mall/user/validation/password_min_length", dataType: "I32", defaultValue: "6"},
		{configKey: "user.validation.username_max_length", registryPath: "/config/douyin-mall/user/validation/username_max_length", dataType: "I32", defaultValue: "50"},
		{configKey: "user.validation.username_min_length", registryPath: "/config/douyin-mall/user/validation/username_min_length", dataType: "I32", defaultValue: "3"},
	}
}

// TestRegistryAlignment 验证 adapter 硬编码的路径和默认值与 config_metadata.json 一致。
func TestRegistryAlignment(t *testing.T) {
	registry := loadRegistry(t)
	if registry == nil {
		return // 文件未找到时跳过
	}

	// 构建 registry 查找表
	registryByKey := make(map[string]registryEntry)
	for _, e := range registry {
		registryByKey[e.ConfigKey] = e
	}

	adapters := buildAdapterDefaults()

	// 验证所有 adapter 条目都能在 registry 中找到
	// 并且 registry_path / default_value 匹配
	for _, a := range adapters {
		t.Run(a.configKey, func(t *testing.T) {
			entry, ok := registryByKey[a.configKey]
			if !ok {
				t.Fatalf("config_key %q not found in config_metadata.json", a.configKey)
			}

			// 验证 registry_path
			if entry.RegistryPath != a.registryPath {
				t.Errorf("registry_path mismatch:\n  got:      %q\n  expected: %q",
					a.registryPath, entry.RegistryPath)
			}

			// 验证 data_type
			if entry.DataType != a.dataType {
				t.Errorf("data_type mismatch for %q:\n  got:      %q\n  expected: %q",
					a.configKey, a.dataType, entry.DataType)
			}

			// 验证 default_value（忽略 JSON 格式差异，语义相等即可）
			if !defaultValuesEqual(entry.DefaultValue, a.defaultValue) {
				t.Errorf("default_value mismatch for %q:\n  registry: %q\n  adapter:  %q",
					a.configKey, entry.DefaultValue, a.defaultValue)
			}
		})
	}

	// 验证 registry 中没有遗漏的配置
	adapterKeys := make(map[string]bool)
	for _, a := range adapters {
		adapterKeys[a.configKey] = true
	}
	for _, e := range registry {
		if !adapterKeys[e.ConfigKey] {
			t.Errorf("registry %q (%s) is NOT covered by any adapter", e.ConfigKey, e.RegistryPath)
		}
	}
}

// defaultValuesEqual 比较两个默认值字符串是否语义相等。
// 对于 JSON 字符串，尝试解析后比较；否则直接字符串比较。
func defaultValuesEqual(a, b string) bool {
	if a == b {
		return true
	}

	// 尝试 JSON 解析后比较
	var va, vb interface{}
	if err := json.Unmarshal([]byte(a), &va); err != nil {
		return false // 不是 JSON，且不相等
	}
	if err := json.Unmarshal([]byte(b), &vb); err != nil {
		return false
	}
	return reflect.DeepEqual(va, vb)
}



// TestRegistryCoverage 验证 registry 与 adapter 之间无遗漏、无多余。
func TestRegistryCoverage(t *testing.T) {
	registry := loadRegistry(t)
	if registry == nil {
		return
	}

	adapters := buildAdapterDefaults()

	// adapter 不能有多余的条目（不在 registry 中的）
	registryKeys := make(map[string]bool)
	for _, e := range registry {
		registryKeys[e.ConfigKey] = true
	}
	for _, a := range adapters {
		if !registryKeys[a.configKey] {
			t.Errorf("adapter contains %q which is NOT in config_metadata.json", a.configKey)
		}
	}

	// registry 中的每个条目必须被 adapter 覆盖
	adapterKeys := make(map[string]bool)
	for _, a := range adapters {
		adapterKeys[a.configKey] = true
	}
	for _, e := range registry {
		if !adapterKeys[e.ConfigKey] {
			t.Errorf("registry %q (%s) is NOT covered by any adapter", e.ConfigKey, e.RegistryPath)
		}
	}
}

// TestRegistryPathFormat 验证所有 registry_path 格式正确。
func TestRegistryPathFormat(t *testing.T) {
	registry := loadRegistry(t)
	if registry == nil {
		return
	}

	expectedPrefix := "/config/douyin-mall/"
	for _, e := range registry {
		t.Run(e.ConfigKey, func(t *testing.T) {
			if len(e.RegistryPath) <= len(expectedPrefix) || e.RegistryPath[:len(expectedPrefix)] != expectedPrefix {
				t.Errorf("registry_path %q does not start with %q", e.RegistryPath, expectedPrefix)
			}
		})
	}
}

// TestAdapterPathsSorted 验证 adapter 路径按 config_key 排序（供审查用）。
func TestAdapterPathsSorted(t *testing.T) {
	adapters := buildAdapterDefaults()
	keys := make([]string, len(adapters))
	for i, a := range adapters {
		keys[i] = a.configKey
	}
	if !sort.StringsAreSorted(keys) {
		t.Error("adapter defaults are not sorted by config_key")
	}
}
