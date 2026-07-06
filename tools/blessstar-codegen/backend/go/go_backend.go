package go_backend

import (
	"fmt"
	"sort"
	"strings"

	"github.com/blessstar/blessstar-codegen/backend"
	"github.com/blessstar/blessstar-codegen/types"
)

// GoBackend implements the LanguageBackend for Go
type GoBackend struct{}

func New() *GoBackend { return &GoBackend{} }

func (g *GoBackend) Name() string           { return "go" }
func (g *GoBackend) FileExtension() string  { return ".go" }
func (g *GoBackend) CommentPrefix() string  { return "//" }

// ConfigDomainPortName maps a Chinese business domain name to a Go port interface name
func ConfigDomainPortName(domain string) string {
	// Map known Chinese domain names to English port names
	domainMap := map[string]string{
		"认证鉴权": "Auth",
		"用户管理": "User",
		"安全策略": "Cors",
		"订单管理": "Order",
		"支付管理": "Payment",
		"商品管理": "Product",
		"评价管理": "Review",
		"未分类":   "Misc",
	}
	if name, ok := domainMap[domain]; ok {
		return name
	}
	// Fallback: CamelCase the domain name
	return strings.ReplaceAll(domain, " ", "")
}

// PortInterfaceName returns the full Go interface name
func PortInterfaceName(domain string) string {
	return ConfigDomainPortName(domain) + "Config"
}

// PortFileName returns the file name for the port
func PortFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + ".go"
}

// AdapterTypeName returns the Go struct name for the BlessStar adapter
func AdapterTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigAdapter"
}

// MockTypeName returns the Go struct name for the mock adapter
func MockTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigMock"
}

// AdapterFileName returns the file name for the adapter
func AdapterFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "_adapter.go"
}

// MockFileName returns the file name for the mock
func MockFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "_mock.go"
}

// PackageNameFromBiz returns a valid Go package name from biz_id.
// Replaces hyphens with underscores since Go identifiers cannot contain hyphens.
func PackageNameFromBiz(bizID string) string {
	return strings.ReplaceAll(bizID, "-", "_")
}

// GoType maps a BlessStar type to Go
func GoType(bsType string) string {
	return types.GoBlessStarType(bsType)
}

// toCamelCase converts a snake_case or dot-separated string to CamelCase.
// Examples: "token_expiry_seconds" → "TokenExpirySeconds", "jwt" → "Jwt"
func toCamelCase(s string) string {
	// Split by both underscore and dot
	parts := strings.FieldsFunc(s, func(r rune) bool {
		return r == '_' || r == '.'
	})
	for i, p := range parts {
		if len(p) > 0 {
			parts[i] = string(p[0]-32) + p[1:] // strings.Title replacement
		}
	}
	return strings.Join(parts, "")
}

// parseConfigKeyToMethod converts a config key like "auth.jwt.token_expiry_seconds" to a Go method name
func parseConfigKeyToMethod(key string) string {
	return toCamelCase(key)
}

// MethodNameFromKey maps a config key to a Go method name
func MethodNameFromKey(key string) string {
	parts := strings.Split(key, ".")
	// Remove domain prefix (e.g., "auth") to get the method name
	if len(parts) >= 2 {
		parts = parts[1:]
	}
	var camelParts []string
	for _, p := range parts {
		camelParts = append(camelParts, toCamelCase(p))
	}
	return strings.Join(camelParts, "")
}

// LabelFromKey returns the config label from the label map or auto-generates
func LabelFromKey(key string, labels map[string]string) string {
	if label, ok := labels[key]; ok && label != "" {
		return label
	}
	parts := strings.Split(key, ".")
	lastPart := strings.ReplaceAll(parts[len(parts)-1], "_", " ")
	if len(lastPart) > 0 {
		lastPart = string(lastPart[0]-32) + lastPart[1:]
	}
	return lastPart
}

// generateFieldDoc generates Go comment for a config field
func generateFieldDoc(field types.ConfigField, labels map[string]string) string {
	label := LabelFromKey(field.Key, labels)
	desc := field.Description
	if desc == "" {
		desc = field.AIHint
	}
	rangeHint := field.ValueRange

	var lines []string
	lines = append(lines, fmt.Sprintf("// %s (%s)", label, field.Key))
	if desc != "" {
		lines = append(lines, fmt.Sprintf("// 描述: %s", desc))
	}
	if rangeHint != "" {
		lines = append(lines, fmt.Sprintf("// 建议值: %s", rangeHint))
	}
	lines = append(lines, fmt.Sprintf("// 类型: %s", field.Type))
	return strings.Join(lines, "\n")
}

func (g *GoBackend) GeneratePortInterface(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	packageName := "ports"

	// Determine if any config in this domain uses time.Duration return type
	needsTime := false
	for _, c := range configs {
		if strings.Contains(strings.ToLower(c.Key), "timeout") ||
			strings.Contains(strings.ToLower(c.Key), "expiry") ||
			strings.Contains(strings.ToLower(c.Key), "ttl") ||
			strings.Contains(strings.ToLower(c.Key), "duration") {
			needsTime = true
			break
		}
	}

	var b strings.Builder
	b.WriteString(fmt.Sprintf("// Package ports 自动生成于 BlessStar 配置端口-适配器\n"))
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("// 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n"))
	b.WriteString(fmt.Sprintf("// Source: manifest.json\n\n"))
	b.WriteString(fmt.Sprintf("package %s\n\n", packageName))
	if needsTime {
		b.WriteString("import (\n\t\"context\"\n\t\"time\"\n)\n\n")
	} else {
		b.WriteString("import (\n\t\"context\"\n)\n\n")
	}
	b.WriteString(fmt.Sprintf("// %s %s 域配置接口\n", interfaceName, domain))
	b.WriteString(fmt.Sprintf("// 对应 domain: %q\n", domain))
	b.WriteString("// 禁止直接 import BlessStar SDK — 请通过此接口访问配置\n")
	b.WriteString(fmt.Sprintf("type %s interface {\n", interfaceName))

	for _, c := range configs {
		goType := GoType(c.Type)
		methodName := MethodNameFromKey(c.Key)

		b.WriteString("\n")
		b.WriteString(fmt.Sprintf("\t// %s\n", generateFieldDoc(c, biz.ConfigLabels)))
		b.WriteString(fmt.Sprintf("\t// Returns: %s\n", goType))

		// Handle time.Duration return for time-related configs
		if strings.Contains(strings.ToLower(c.Key), "timeout") ||
			strings.Contains(strings.ToLower(c.Key), "expiry") ||
			strings.Contains(strings.ToLower(c.Key), "ttl") ||
			strings.Contains(strings.ToLower(c.Key), "duration") {
			b.WriteString(fmt.Sprintf("\t%s(ctx context.Context) (time.Duration, error)\n", methodName))
		} else {
			b.WriteString(fmt.Sprintf("\t%s(ctx context.Context) (%s, error)\n", methodName, goType))
		}
	}

	b.WriteString("}\n")

	return &backend.File{
		Path:    fmt.Sprintf("ports/%s", PortFileName(domain)),
		Content: b.String(),
	}, nil
}

func (g *GoBackend) GenerateBlessStarAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	adapterName := AdapterTypeName(domain)
	packageName := "adapter_blessstar"

	// Determine if any config in this domain uses time.Duration return type
	needsTime := false
	for _, c := range configs {
		if strings.Contains(strings.ToLower(c.Key), "timeout") ||
			strings.Contains(strings.ToLower(c.Key), "expiry") ||
			strings.Contains(strings.ToLower(c.Key), "ttl") ||
			strings.Contains(strings.ToLower(c.Key), "duration") {
			needsTime = true
			break
		}
	}

	var b strings.Builder
	b.WriteString(fmt.Sprintf("// Package %s 自动生成于 BlessStar 配置端口-适配器\n", packageName))
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("// 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n"))
	b.WriteString(fmt.Sprintf("package %s\n\n", packageName))
	b.WriteString(fmt.Sprintf("import (\n\t\"context\"\n\t\"sync\"\n\n"))
	if needsTime {
		b.WriteString("\t\"time\"\n\n")
	}
	b.WriteString(fmt.Sprintf("\t\"%s/ports\"\n", biz.BizID))
	b.WriteString(")\n\n")

	// Struct definition with 3-stage fallback using ConfigReader
	b.WriteString(fmt.Sprintf("// %s %s 域配置的 BlessStar 适配器\n", adapterName, domain))
	b.WriteString(fmt.Sprintf("// 内置三阶段降级: ConfigReader实时查询 → LastKnownGood缓存 → 硬编码默认值\n"))
	b.WriteString(fmt.Sprintf("type %s struct {\n", adapterName))
	b.WriteString("\treader          ports.ConfigReader\n")
	b.WriteString("\tlastKnownCache  sync.Map\n")
	b.WriteString("\thardcodedDefaults  map[string]interface{}\n")
	b.WriteString("}\n\n")

	// Constructor — accepts ports.ConfigReader instead of blessstar.Client
	b.WriteString(fmt.Sprintf("// New%s 创建 %s 适配器实例\n", adapterName, adapterName))
	b.WriteString(fmt.Sprintf("// reader 参数是配置读取器，由业务方注入（可为 CachedReader、HTTPReader 等实现）\n"))
	b.WriteString(fmt.Sprintf("func New%s(reader ports.ConfigReader) ports.%s {\n", adapterName, interfaceName))
	b.WriteString(fmt.Sprintf("\treturn &%s{\n", adapterName))
	b.WriteString("\t\treader: reader,\n")
	b.WriteString("\t\thardcodedDefaults: map[string]interface{}{\n")
	for _, c := range configs {
		goType := GoType(c.Type)
		methodName := MethodNameFromKey(c.Key)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = types.GoBlessStarTypeZero(goType)
		}
		// Duration 字段需要在硬编码默认值中加入 time.Duration() 包装
		// 以匹配接口方法签名中的 time.Duration 返回类型
		isDuration := strings.Contains(strings.ToLower(c.Key), "timeout") ||
			strings.Contains(strings.ToLower(c.Key), "expiry") ||
			strings.Contains(strings.ToLower(c.Key), "ttl") ||
			strings.Contains(strings.ToLower(c.Key), "duration")
		defaultGoType := goType
		if isDuration {
			defaultGoType = "time.Duration"
		}
		b.WriteString(fmt.Sprintf("\t\t\t%q: %s,\n", methodName, formatGoDefault(defaultGoType, defaultVal)))
	}
	b.WriteString("\t\t},\n")
	b.WriteString("\t}\n")
	b.WriteString("}\n\n")

	// Method implementations
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		goType := GoType(c.Type)

		// Determine return type (time.Duration for time-related configs)
		returnType := goType
		isDuration := strings.Contains(strings.ToLower(c.Key), "timeout") ||
			strings.Contains(strings.ToLower(c.Key), "expiry") ||
			strings.Contains(strings.ToLower(c.Key), "ttl") ||
			strings.Contains(strings.ToLower(c.Key), "duration")
		if isDuration {
			returnType = "time.Duration"
		}

		// Method signature
		b.WriteString(fmt.Sprintf("func (a *%s) %s(ctx context.Context) (%s, error) {\n", adapterName, methodName, returnType))

		// Step 1: Try ConfigReader (SHM / HTTP / env / ...)
		registryPath := c.RegistryPath
		if registryPath == "" {
			registryPath = fmt.Sprintf("/config/%s/%s", biz.BizID, strings.ReplaceAll(c.Key, ".", "/"))
		}
		b.WriteString(fmt.Sprintf("\t// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）\n"))
		b.WriteString(fmt.Sprintf("\tval, err := a.reader.Get(ctx, %q)\n", registryPath))
		b.WriteString(fmt.Sprintf("\tif err == nil {\n"))
		b.WriteString(fmt.Sprintf("\t\ta.lastKnownCache.Store(%q, val)\n", methodName))
		b.WriteString(fmt.Sprintf("\t\treturn val.(%s), nil\n", returnType))
		b.WriteString("\t}\n\n")

		// Step 2: Try last known good cache
		b.WriteString(fmt.Sprintf("\t// 第2阶段: 降级到 Last Known Good 缓存\n"))
		b.WriteString(fmt.Sprintf("\tif cached, ok := a.lastKnownCache.Load(%q); ok {\n", methodName))
		b.WriteString(fmt.Sprintf("\t\treturn cached.(%s), nil\n", returnType))
		b.WriteString("\t}\n\n")

		// Step 3: Hardcoded default
		b.WriteString(fmt.Sprintf("\t// 第3阶段: 极冷启动 — 返回硬编码默认值\n"))
		b.WriteString(fmt.Sprintf("\treturn a.hardcodedDefaults[%q].(%s), nil\n", methodName, returnType))
		b.WriteString("}\n\n")
	}

	return &backend.File{
		Path:    fmt.Sprintf("adapters/blessstar/%s", AdapterFileName(domain)),
		Content: b.String(),
	}, nil
}

func (g *GoBackend) GenerateMockAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	mockName := MockTypeName(domain)
	packageName := "adapter_mock"

	// Determine if any config in this domain uses time.Duration return type
	needsTime := false
	for _, c := range configs {
		if strings.Contains(strings.ToLower(c.Key), "timeout") ||
			strings.Contains(strings.ToLower(c.Key), "expiry") ||
			strings.Contains(strings.ToLower(c.Key), "ttl") ||
			strings.Contains(strings.ToLower(c.Key), "duration") {
			needsTime = true
			break
		}
	}

	var b strings.Builder
	b.WriteString(fmt.Sprintf("// Package %s 自动生成于 BlessStar 配置 Mock\n", packageName))
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("// 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf("// 专为单元测试设计 — 固定返回值\n\n"))
	b.WriteString(fmt.Sprintf("package %s\n\n", packageName))
	if needsTime {
		b.WriteString(fmt.Sprintf("import (\n\t\"context\"\n\t\"time\"\n\t\"%s/ports\"\n)\n\n", biz.BizID))
	} else {
		b.WriteString(fmt.Sprintf("import (\n\t\"context\"\n\t\"%s/ports\"\n)\n\n", biz.BizID))
	}
	b.WriteString(fmt.Sprintf("// %s %s 域配置的 Mock 实现（单元测试用）\n", mockName, domain))
	b.WriteString(fmt.Sprintf("type %s struct {\n", mockName))

	// Compute return type for each config
	type configReturn struct {
		fieldName    string
		goType       string
		returnType   string
		isDuration   bool
	}
	configReturns := make([]configReturn, len(configs))
	for i, c := range configs {
		gt := GoType(c.Type)
		isDur := strings.Contains(strings.ToLower(c.Key), "timeout") ||
			strings.Contains(strings.ToLower(c.Key), "expiry") ||
			strings.Contains(strings.ToLower(c.Key), "ttl") ||
			strings.Contains(strings.ToLower(c.Key), "duration")
		rt := gt
		if isDur {
			rt = "time.Duration"
		}
		configReturns[i] = configReturn{
			fieldName:  MethodNameFromKey(c.Key),
			goType:     gt,
			returnType: rt,
			isDuration: isDur,
		}
	}

	// Fields for each config
	for _, cr := range configReturns {
		b.WriteString(fmt.Sprintf("\t%sFunc func(ctx context.Context) (%s, error)\n", cr.fieldName, cr.returnType))
	}
	b.WriteString("}\n\n")

	// Constructor with default values
	b.WriteString(fmt.Sprintf("// New%s 创建默认 Mock（返回值按 manifest 默认值设定）\n", mockName))
	b.WriteString(fmt.Sprintf("func New%s() ports.%s {\n", mockName, interfaceName))
	b.WriteString(fmt.Sprintf("\treturn &%s{\n", mockName))
	for i, c := range configs {
		cr := configReturns[i]
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = types.GoBlessStarTypeZero(cr.goType)
		}
		b.WriteString(fmt.Sprintf("\t\t%sFunc: func(ctx context.Context) (%s, error) {\n", cr.fieldName, cr.returnType))
		b.WriteString(fmt.Sprintf("\t\t\treturn %s, nil\n", formatGoDefault(cr.goType, defaultVal)))
		b.WriteString("\t\t},\n")
	}
	b.WriteString("\t}\n")
	b.WriteString("}\n\n")

	// Method implementations
	for _, cr := range configReturns {
		b.WriteString(fmt.Sprintf("func (m *%s) %s(ctx context.Context) (%s, error) {\n", mockName, cr.fieldName, cr.returnType))
		b.WriteString(fmt.Sprintf("\treturn m.%sFunc(ctx)\n", cr.fieldName))
		b.WriteString("}\n\n")
	}

	return &backend.File{
		Path:    fmt.Sprintf("adapters/mock/%s", MockFileName(domain)),
		Content: b.String(),
	}, nil
}

func (g *GoBackend) GenerateProvider(biz *types.BizSystem) (*backend.File, error) {
	packageName := PackageNameFromBiz(biz.BizID)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("// Package %s 自动生成于 BlessStar 配置依赖注入\n", packageName))
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n"))
	b.WriteString(fmt.Sprintf("package %s\n\n", packageName))
	b.WriteString(fmt.Sprintf("import (\n"))
	b.WriteString(fmt.Sprintf("\t\"%s/ports\"\n", biz.BizID))
	b.WriteString(fmt.Sprintf("\t\"%s/adapters/blessstar\"\n", biz.BizID))
	b.WriteString(")\n\n")

	// Adapters struct
	b.WriteString("// Adapters 持有所有域的配置适配器\n")
	b.WriteString("type Adapters struct {\n")
	sortedDomains := sortBizDomains(biz)
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		b.WriteString(fmt.Sprintf("\t%s ports.%s\n", portName, portName))
	}
	b.WriteString("}\n\n")

	// Provide function — accepts ports.ConfigReader instead of *blessstar.Client
	b.WriteString("// ProvideBlessStarAdapters 一行实例化所有域配置适配器\n")
	b.WriteString("// 注入 ConfigReader 后返回所有域的 Port 实现\n")
	b.WriteString("// reader 由业务方提供（可为 CachedReader、HTTPReader、EnvReader 等实现）\n")
	b.WriteString("func ProvideBlessStarAdapters(reader ports.ConfigReader) *Adapters {\n")
	b.WriteString("\treturn &Adapters{\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		adapterName := AdapterTypeName(d)
		b.WriteString(fmt.Sprintf("\t\t%s: adapter_blessstar.New%s(reader),\n", portName, adapterName))
	}
	b.WriteString("\t}\n")
	b.WriteString("}\n\n")

	return &backend.File{
		Path:    "provider/blessstar_provider.go",
		Content: b.String(),
	}, nil
}

func (g *GoBackend) GenerateGoMod(biz *types.BizSystem) (*backend.File, error) {
	content := fmt.Sprintf(`module %s

go 1.21

// 零外部依赖 — ConfigReader 由业务方自行实现
// 无外部 SDK 依赖，生成的 adapter 代码仅依赖 Go 标准库和本 module
`, biz.BizID)

	return &backend.File{
		Path:    "go.mod",
		Content: content,
	}, nil
}

func (g *GoBackend) GenerateConfigReaderFile(biz *types.BizSystem) ([]*backend.File, error) {
	pkgName := PackageNameFromBiz(biz.BizID)

	// Generate ports/config_reader.go — the ConfigReader interface
	configReaderContent := fmt.Sprintf(`// Package ports 提供业务系统的配置读取接口。
// 所有 Port 接口定义在此包中，业务代码仅依赖此包。
// 请勿手动修改 — 由 blessstar-codegen 自动生成
package ports

import "context"

// ConfigReader 是配置读取接口，业务方自行选择实现方式。
// 内置实现包括：CachedReader（秒级轮询缓存）、EnvReader（环境变量）、FileReader（本地文件）等。
//
// 实现类通过环境变量 BLESSSTAR_ENDPOINT 获取 Electron 地址（HTTPReader 场景）。
// 初始化失败时不得阻塞进程，由 adapter 的三阶段降级兜底。
//
// Get 返回 interface{} 以便 adapter 进行类型断言（与三阶段降级兼容）。
// 常见返回类型：int64, bool, string, []string, time.Duration。
// 业务方也可选择返回 JSON string 并在 adapter 外部自行解析。
type ConfigReader interface {
	// Get 读取一个配置值。path 是配置的完整注册路径（如 "/config/%[1]s/auth/jwt/token_expiry_seconds"）。
	// 返回配置值（可直接类型断言）或 error。
	Get(ctx context.Context, path string) (interface{}, error)
}
`, biz.BizID)

	// Generate provider/cached_reader.go — optional CachedReader decorator
	cachedReaderContent := fmt.Sprintf(`// Package %[1]s 提供 BlessStar 配置的依赖注入和可选装饰器。
// 请勿手动修改 — 由 blessstar-codegen 自动生成
package %[1]s

import (
	"context"
	"sync"
	"time"

	"%[2]s/ports"
)

// CachedReader 是对 ConfigReader 的缓存包装器（可选装饰器）。
// 后台协程使用 time.Ticker 定时刷新缓存，实现秒级准实时热更新。
// 不依赖任何外部库，仅使用 Go 标准库。
//
// 使用方式（由 main.go 注入）：
//
//	rawReader := httpreader.New()                       // 从环境变量 BLESSSTAR_ENDPOINT 读取地址
//	cachedReader := provider.NewCachedReader(rawReader, 30*time.Second)
//	adapters := provider.ProvideBlessStarAdapters(cachedReader)
type CachedReader struct {
	inner       ports.ConfigReader
	cache       sync.Map
	ticker      *time.Ticker
	refreshFunc func(ctx context.Context) error
}

// NewCachedReader 创建 CachedReader 实例。
// interval 控制缓存的刷新周期（如 30 秒）。
// 业务方可设置 RefreshFunc 自定义全量刷新逻辑。
func NewCachedReader(inner ports.ConfigReader, interval time.Duration) *CachedReader {
	cr := &CachedReader{
		inner:       inner,
		ticker:      time.NewTicker(interval),
		refreshFunc: func(ctx context.Context) error { return nil }, // 默认空操作
	}
	go cr.refreshLoop(context.Background())
	return cr
}

// RefreshFunc 设置全量刷新回调函数，由业务方自定义需要缓存的配置路径。
func (c *CachedReader) RefreshFunc(fn func(ctx context.Context) error) *CachedReader {
	c.refreshFunc = fn
	return c
}

// Get 从缓存中读取配置值。若缓存命中直接返回，否则穿透到 inner.ConfigReader。
func (c *CachedReader) Get(ctx context.Context, path string) (interface{}, error) {
	if val, ok := c.cache.Load(path); ok {
		return val, nil
	}
	return c.inner.Get(ctx, path)
}

// refreshLoop 后台协程，定时执行全量刷新。
func (c *CachedReader) refreshLoop(ctx context.Context) {
	for range c.ticker.C {
		if err := c.refreshFunc(ctx); err != nil {
			// 刷新失败不影响现有缓存，仅跳过本轮
			continue
		}
	}
}

// Close 停止后台刷新协程。
func (c *CachedReader) Close() {
	c.ticker.Stop()
}
`, pkgName, biz.BizID)

	return []*backend.File{
		{
			Path:    "ports/config_reader.go",
			Content: configReaderContent,
		},
		{
			Path:    "provider/cached_reader.go",
			Content: cachedReaderContent,
		},
	}, nil
}

// sortBizDomains returns sorted domain names from BizSystem
func sortBizDomains(biz *types.BizSystem) []string {
	var domains []string
	for d := range biz.ConfigsByDomain {
		domains = append(domains, d)
	}
	sort.Strings(domains)
	return domains
}

// formatGoDefault formats a default value as a Go literal
func formatGoDefault(goType, defaultVal string) string {
	switch {
	case goType == "int64":
		if defaultVal == "" {
			return "int64(0)"
		}
		return fmt.Sprintf("int64(%s)", defaultVal)
	case goType == "int32":
		if defaultVal == "" {
			return "int32(0)"
		}
		return fmt.Sprintf("int32(%s)", defaultVal)
	case goType == "bool":
		if defaultVal == "true" || defaultVal == "1" {
			return "true"
		}
		return "false"
	case goType == "string":
		if defaultVal == "" {
			return `""`
		}
		return fmt.Sprintf("%q", defaultVal)
	case goType == "time.Duration":
		if defaultVal == "" {
			return "time.Duration(0)"
		}
		return fmt.Sprintf("time.Duration(%s)", defaultVal)
	case goType == "[]string":
		// Parse JSON array like ["*", "example.com"] → []string{"*", "example.com"}
		if defaultVal == "" || defaultVal == "null" {
			return "nil"
		}
		// Remove brackets and split by comma
		trimmed := strings.TrimSpace(defaultVal)
		trimmed = strings.TrimPrefix(trimmed, "[")
		trimmed = strings.TrimSuffix(trimmed, "]")
		if trimmed == "" {
			return "nil"
		}
		parts := strings.Split(trimmed, ",")
		var sb strings.Builder
		sb.WriteString("[]string{")
		for i, p := range parts {
			if i > 0 {
				sb.WriteString(", ")
			}
			// Remove surrounding quotes and spaces
			elem := strings.TrimSpace(p)
			elem = strings.Trim(elem, `"`)
			sb.WriteString(fmt.Sprintf("%q", elem))
		}
		sb.WriteString("}")
		return sb.String()
	default:
		if defaultVal == "" {
			return `""`
		}
		return fmt.Sprintf("%q", defaultVal)
	}
}

// Ensure all interfaces are satisfied
var _ backend.LanguageBackend = (*GoBackend)(nil)
