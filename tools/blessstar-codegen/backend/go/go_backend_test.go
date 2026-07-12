package go_backend

import (
	"strings"
	"testing"

	"github.com/blessstar/blessstar-codegen/types"
)

func TestConfigDomainPortName(t *testing.T) {
	tests := []struct {
		domain string
		want   string
	}{
		{"认证鉴权", "Auth"},
		{"用户管理", "User"},
		{"安全策略", "Cors"},
		{"订单管理", "Order"},
		{"支付管理", "Payment"},
		{"商品管理", "Product"},
		{"评价管理", "Review"},
		{"未分类", "Misc"},
		{"自定义域", "自定义域"}, // fallback
	}
	for _, tt := range tests {
		got := ConfigDomainPortName(tt.domain)
		if got != tt.want {
			t.Errorf("ConfigDomainPortName(%q) = %q, want %q", tt.domain, got, tt.want)
		}
	}
}

func TestPortInterfaceName(t *testing.T) {
	if name := PortInterfaceName("认证鉴权"); name != "AuthConfig" {
		t.Errorf("PortInterfaceName = %q, want %q", name, "AuthConfig")
	}
}

func TestAdapterTypeName(t *testing.T) {
	if name := AdapterTypeName("认证鉴权"); name != "AuthConfigAdapter" {
		t.Errorf("AdapterTypeName = %q, want %q", name, "AuthConfigAdapter")
	}
}

func TestMockTypeName(t *testing.T) {
	if name := MockTypeName("认证鉴权"); name != "AuthConfigMock" {
		t.Errorf("MockTypeName = %q, want %q", name, "AuthConfigMock")
	}
}

func TestMethodNameFromKey(t *testing.T) {
	tests := []struct {
		key  string
		want string
	}{
		{"auth.jwt.token_expiry_seconds", "JwtTokenExpirySeconds"},
		{"user.role.values", "RoleValues"},
		{"cors.allowed_origins", "AllowedOrigins"},
		{"review.rating.max", "RatingMax"},
	}
	for _, tt := range tests {
		got := MethodNameFromKey(tt.key)
		if got != tt.want {
			t.Errorf("MethodNameFromKey(%q) = %q, want %q", tt.key, got, tt.want)
		}
	}
}

func TestGeneratePortInterface(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID:       "douyin-mall",
		DisplayName: "抖音商城",
		ConfigLabels: map[string]string{
			"auth.jwt.token_expiry_seconds": "JWT过期时间",
		},
	}

	configs := []types.ConfigField{
		{
			Key:         "auth.jwt.token_expiry_seconds",
			Type:        "I64",
			Default:     "86400",
			Description: "JWT登录令牌的有效期时长（秒）",
			AIHint:      "JWT token过期时间配置",
			ValueRange:  "3600~604800",
		},
	}

	file, err := gen.GeneratePortInterface(biz, "认证鉴权", configs)
	if err != nil {
		t.Fatalf("GeneratePortInterface failed: %v", err)
	}
	if file == nil {
		t.Fatal("GeneratePortInterface returned nil file")
	}

	content := file.Content
	if len(content) == 0 {
		t.Fatal("Generated content is empty")
	}

	// Verify key elements
	checks := []string{
		"package ports",
		"import (",
		"\"context\"",
		"\"time\"",
		"AuthConfig",
		"TokenExpirySeconds",
	}
	for _, c := range checks {
		if !contains(content, c) {
			t.Errorf("Generated content missing: %s", c)
		}
	}
}

func TestGenerateBlessStarAdapter(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID:       "douyin-mall",
		DisplayName: "抖音商城",
		ConfigLabels: map[string]string{
			"auth.jwt.token_expiry_seconds": "JWT过期时间",
		},
	}

	configs := []types.ConfigField{
		{
			Key:          "auth.jwt.token_expiry_seconds",
			Type:         "I64",
			Default:      "86400",
			Description:  "JWT登录令牌的有效期时长（秒）",
			RegistryPath: "/config/douyin-mall/auth/jwt/token_expiry_seconds",
		},
	}

	file, err := gen.GenerateBlessStarAdapter(biz, "认证鉴权", configs)
	if err != nil {
		t.Fatalf("GenerateBlessStarAdapter failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateBlessStarAdapter returned nil file")
	}

	content := file.Content

	// Verify ConfigReader is used instead of blessstar.Client
	if !contains(content, "ports.ConfigReader") {
		t.Error("Adapter should reference ports.ConfigReader")
	}
	if contains(content, "blessstar-sdk-go") {
		t.Error("Adapter should NOT import blessstar-sdk-go")
	}
	if contains(content, "blessstar.Client") {
		t.Error("Adapter should NOT reference blessstar.Client")
	}
	if !contains(content, "a.reader.Get(ctx") {
		t.Error("Adapter should call a.reader.Get(ctx, ...)")
	}

	// Verify 3-stage fallback is present
	stageChecks := []string{
		"第1阶段: ConfigReader 实时查询",
		"第2阶段: 降级到 Last Known Good 缓存",
		"第3阶段: 极冷启动 — 返回硬编码默认值",
		"lastKnownCache",
		"hardcodedDefaults",
		"sync.Map",
	}
	for _, c := range stageChecks {
		if !contains(content, c) {
			t.Errorf("Adapter missing critical fallback component: %s", c)
		}
	}
}

func TestGenerateMockAdapter(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID: "douyin-mall",
		DisplayName: "抖音商城",
	}

	configs := []types.ConfigField{
		{
			Key:     "auth.jwt.token_expiry_seconds",
			Type:    "I64",
			Default: "86400",
		},
	}

	file, err := gen.GenerateMockAdapter(biz, "认证鉴权", configs)
	if err != nil {
		t.Fatalf("GenerateMockAdapter failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateMockAdapter returned nil file")
	}

	content := file.Content
	checks := []string{
		"AuthConfigMock",
		"TokenExpirySecondsFunc",
		"func(ctx context.Context)",
	}
	for _, c := range checks {
		if !contains(content, c) {
			t.Errorf("Mock missing: %s", c)
		}
	}
}

func TestGenerateProvider(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID:       "douyin-mall",
		DisplayName: "抖音商城",
		ConfigsByDomain: map[string][]types.ConfigField{
			"认证鉴权": {
				{Key: "auth.jwt.token_expiry_seconds", Type: "I64"},
			},
			"用户管理": {
				{Key: "user.role.values", Type: "ENUM"},
			},
		},
	}

	file, err := gen.GenerateProvider(biz)
	if err != nil {
		t.Fatalf("GenerateProvider failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateProvider returned nil file")
	}

	content := file.Content
	checks := []string{
		"ProvideBlessStarAdapters",
		"type Adapters struct",
		"AuthConfig ports.AuthConfig",
		"UserConfig ports.UserConfig",
	}
	for _, c := range checks {
		if !contains(content, c) {
			t.Errorf("Provider missing: %s", c)
		}
	}
	if contains(content, "blessstar-sdk-go") {
		t.Error("Provider should NOT import blessstar-sdk-go")
	}
	if contains(content, "blessstar.Client") {
		t.Error("Provider should NOT reference blessstar.Client")
	}
	if !contains(content, "reader ports.ConfigReader") {
		t.Error("Provider should accept ports.ConfigReader")
	}
}

func TestGenerateGoMod(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{BizID: "douyin-mall"}

	file, err := gen.GenerateGoMod(biz)
	if err != nil {
		t.Fatalf("GenerateGoMod failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateGoMod returned nil file")
	}

	if !contains(file.Content, "module douyin-mall") {
		t.Error("go.mod missing module declaration")
	}
	if strings.Contains(file.Content, "require (") && strings.Contains(file.Content, "blessstar-sdk-go") {
		t.Error("go.mod should NOT require blessstar-sdk-go")
	}
	if strings.Contains(file.Content, "replace blessstar-sdk-go") {
		t.Error("go.mod should NOT have replace directive for blessstar-sdk-go")
	}
}

func TestGenerateConfigReaderFile(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{BizID: "douyin-mall"}

	files, err := gen.GenerateConfigReaderFile(biz)
	if err != nil {
		t.Fatalf("GenerateConfigReaderFile failed: %v", err)
	}
	if len(files) != 2 {
		t.Fatalf("Expected 2 files (config_reader.go + cached_reader.go), got %d", len(files))
	}

	// Check config_reader.go
	crFile := files[0]
	if !contains(crFile.Path, "config_reader.go") {
		t.Errorf("Expected config_reader.go, got %s", crFile.Path)
	}
	if !contains(crFile.Content, "type ConfigReader interface") {
		t.Error("config_reader.go missing ConfigReader interface")
	}
	if !contains(crFile.Content, "Get(ctx context.Context, path string) (interface{}, error)") {
		t.Error("config_reader.go missing Get method signature")
	}
	if !contains(crFile.Content, "BLESSSTAR_ENDPOINT") {
		t.Error("config_reader.go should mention BLESSSTAR_ENDPOINT env var")
	}
	if !contains(crFile.Content, "package ports") {
		t.Error("config_reader.go should be in ports package")
	}

	// Check cached_reader.go
	cdFile := files[1]
	if !contains(cdFile.Path, "cached_reader.go") {
		t.Errorf("Expected cached_reader.go, got %s", cdFile.Path)
	}
	if !contains(cdFile.Content, "type CachedReader struct") {
		t.Error("cached_reader.go missing CachedReader struct")
	}
	if !contains(cdFile.Content, "time.Ticker") {
		t.Error("cached_reader.go should use time.Ticker")
	}
	if !contains(cdFile.Content, "refreshLoop") {
		t.Error("cached_reader.go missing refreshLoop")
	}
	if !contains(cdFile.Content, "NewCachedReader") {
		t.Error("cached_reader.go missing NewCachedReader constructor")
	}
	if !contains(cdFile.Content, "Close()") {
		t.Error("cached_reader.go missing Close method")
	}
}

func TestGenerateEmptyConfigs(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{BizID: "test"}

	// Empty configs should not produce files
	file, err := gen.GeneratePortInterface(biz, "empty", nil)
	if err != nil {
		t.Fatalf("GeneratePortInterface with nil configs: %v", err)
	}
	if file != nil {
		t.Error("Expected nil file for empty configs")
	}
}

func TestFormatGoDefault(t *testing.T) {
	tests := []struct {
		goType string
		val    string
		want   string
	}{
		{"int64", "86400", "int64(86400)"},
		{"int32", "10", "int32(10)"},
		{"bool", "true", "true"},
		{"bool", "false", "false"},
		{"string", "user", "\"user\""},
		{"string", "", "\"\""},
	}
	for _, tt := range tests {
		got := formatGoDefault(tt.goType, tt.val)
		if got != tt.want {
			t.Errorf("formatGoDefault(%q, %q) = %q, want %q", tt.goType, tt.val, got, tt.want)
		}
	}
}

func TestGenerateBlessStarAdapterNoBlessStarSDK(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID:       "douyin-mall",
		DisplayName: "抖音商城",
	}

	configs := []types.ConfigField{
		{
			Key:     "user.role.values",
			Type:    "ENUM",
			Default: "user",
		},
	}

	file, err := gen.GenerateBlessStarAdapter(biz, "用户管理", configs)
	if err != nil {
		t.Fatalf("GenerateBlessStarAdapter failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateBlessStarAdapter returned nil file")
	}

	content := file.Content

	// Verify NOT importing blessstar-sdk-go
	if strings.Contains(content, "blessstar-sdk-go") {
		t.Error("Adapter must NOT import blessstar-sdk-go")
	}
	if strings.Contains(content, "*blessstar.Client") {
		t.Error("Adapter must NOT reference *blessstar.Client")
	}

	// Verify constructor takes ports.ConfigReader
	if !strings.Contains(content, "NewUserConfigAdapter(reader ports.ConfigReader)") {
		t.Error("Constructor must accept ports.ConfigReader")
	}
}

func TestGenerateGateConfigs(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID:       "douyin-mall",
		DisplayName: "抖音商城",
	}

	gateConfigs := []types.GateConfig{
		{FieldKey: "auth.jwt.token_expiry_seconds", GateType: "RANGE", ParamKey: "min", ParamVal: "60"},
		{FieldKey: "auth.jwt.token_expiry_seconds", GateType: "RANGE", ParamKey: "max", ParamVal: "43200"},
		{FieldKey: "auth.jwt.token_expiry_seconds", GateType: "APPROVAL", ParamKey: "required", ParamVal: "true"},
		{FieldKey: "auth.jwt.token_expiry_seconds", GateType: "SLO_WARNING", ParamKey: "slo_impact", ParamVal: "用户连续登录成功率 ≥99.9%"},
		{FieldKey: "payment.timeout", GateType: "DEPENDENCY", ParamKey: "expression", ParamVal: "payment.timeout > payment.max_retry * 2"},
	}

	file, err := gen.GenerateGateConfigs(biz, gateConfigs)
	if err != nil {
		t.Fatalf("GenerateGateConfigs failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateGateConfigs returned nil file")
	}

	content := file.Content
	checks := []string{
		"GATE_CONFIGS_H",
		"AUTH_JWT_TOKEN_EXPIRY_SECONDS_MIN",
		"AUTH_JWT_TOKEN_EXPIRY_SECONDS_MAX",
		"AUTH_JWT_TOKEN_EXPIRY_SECONDS_APPROVAL_REQUIRED",
		"AUTH_JWT_TOKEN_EXPIRY_SECONDS_SLO",
		"GATE_PAYMENT_TIMEOUT_DEP",
	}
	for _, c := range checks {
		if !contains(content, c) {
			t.Errorf("Gate config missing: %s", c)
		}
	}
}

func TestGenerateTestCases(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID:       "douyin-mall",
		DisplayName: "抖音商城",
	}

	testCases := []string{
		`func TestConfig_AuthJwtTokenExpirySeconds_Boundary(t *testing.T) {
	t.Run("within_range", func(t *testing.T) {
		val := int64(60)
		if val < 60 || val > 43200 {
			t.Errorf("value out of range")
		}
	})
}`,
	}

	file, err := gen.GenerateTestCases(biz, testCases)
	if err != nil {
		t.Fatalf("GenerateTestCases failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateTestCases returned nil file")
	}

	content := file.Content
	checks := []string{
		"config_boundary_test.go",
		"package douyin_mall",
		"testing",
	}
	for _, c := range checks {
		if !contains(content, c) {
			t.Errorf("Test cases missing: %s", c)
		}
	}
}

func TestGenerateObservability(t *testing.T) {
	gen := New()
	biz := &types.BizSystem{
		BizID:       "douyin-mall",
		DisplayName: "抖音商城",
	}

	rules := []string{
		`  - alert: AUTH_JWT_TOKEN_EXPIRY_SECONDS_SLO
    expr: rate(blessstar_config_read_total{config_key="auth.jwt.token_expiry_seconds"}[5m]) < 0.999
    for: 1m
    labels:
      severity: warning
    annotations:
      summary: "auth.jwt.token_expiry_seconds: 用户连续登录成功率 ≥99.9%"`,
	}

	file, err := gen.GenerateObservability(biz, rules)
	if err != nil {
		t.Fatalf("GenerateObservability failed: %v", err)
	}
	if file == nil {
		t.Fatal("GenerateObservability returned nil file")
	}

	content := file.Content
	checks := []string{
		"config_alerts.yml",
		"douyin-mall_config_slo",
		"alert: AUTH_JWT_TOKEN_EXPIRY_SECONDS_SLO",
	}
	for _, c := range checks {
		if !contains(content, c) {
			t.Errorf("Observability rules missing: %s", c)
		}
	}
}

func TestSchemaToBizSystem(t *testing.T) {
	// Test SchemaToBizSystem conversion
	schema := &types.ConfigSchema{
		Domain:  "douyin-mall",
		Version: "v1.0.0",
		Fields: []types.ConfigSchemaField{
			{
				Key:          "auth.jwt.token_expiry_seconds",
				Type:         "I64",
				Default:      "11520",
				BusinessDesc: "JWT 访问令牌的有效期时长（分钟）",
				ImpactScope:  []string{"用户登录", "会话管理"},
				Contract: &types.ContractDef{
					Range:            []int64{60, 43200},
					Dependencies:     []string{"auth.jwt.token_expiry_seconds > auth.password.bcrypt_cost * 10"},
					SLOImpact:        "用户连续登录成功率 ≥99.9%",
					ApprovalRequired: true,
				},
				UIMeta: &types.UIMetaDef{
					Label: "Token 过期时间（分钟）",
					Order: 1,
				},
				SearchKeywords: []string{"Token过期", "JWT"},
				AIHint:         "JWT 访问令牌过期时间（分钟）",
			},
		},
	}

	// Test SchemaToConfigField
	field := types.SchemaToConfigField(schema.Fields[0])
	if field.Key != "auth.jwt.token_expiry_seconds" {
		t.Errorf("Key = %q, want %q", field.Key, "auth.jwt.token_expiry_seconds")
	}
	if field.Type != "I64" {
		t.Errorf("Type = %q, want %q", field.Type, "I64")
	}
	if field.Default != "11520" {
		t.Errorf("Default = %q, want %q", field.Default, "11520")
	}
	if field.ValueRange != "60~43200" {
		t.Errorf("ValueRange = %q, want %q", field.ValueRange, "60~43200")
	}
	if field.UIOrder != 1 {
		t.Errorf("UIOrder = %d, want %d", field.UIOrder, 1)
	}
}

func TestExtractGateConfigs(t *testing.T) {
	// Simulate what parser.ExtractGateConfigs would produce
	schema := &types.ConfigSchema{
		Domain: "test",
		Fields: []types.ConfigSchemaField{
			{
				Key: "test.field",
				Contract: &types.ContractDef{
					Range:            []int64{1, 100},
					Dependencies:     []string{"test.field > test.other"},
					SLOImpact:        "SLO: 99.9%",
					ApprovalRequired: true,
				},
			},
		},
	}

	// This test validates the SchemaToConfigField conversion preserves contract info
	field := types.SchemaToConfigField(schema.Fields[0])
	if field.ValueRange != "1~100" {
		t.Errorf("Expected ValueRange '1~100', got %q", field.ValueRange)
	}
}

func contains(s, substr string) bool {
	return len(substr) == 0 || (len(s) >= len(substr) && searchSubstring(s, substr))
}

func searchSubstring(s, sub string) bool {
	for i := 0; i <= len(s)-len(sub); i++ {
		if s[i:i+len(sub)] == sub {
			return true
		}
	}
	return false
}
