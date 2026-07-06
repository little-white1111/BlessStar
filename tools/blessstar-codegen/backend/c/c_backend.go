package c_backend

import (
	"fmt"
	"sort"
	"strings"

	"github.com/blessstar/blessstar-codegen/backend"
	"github.com/blessstar/blessstar-codegen/types"
)

// CBackend implements the LanguageBackend for C
type CBackend struct{}

func New() *CBackend { return &CBackend{} }

func (c *CBackend) Name() string          { return "c" }
func (c *CBackend) FileExtension() string { return ".c" }
func (c *CBackend) CommentPrefix() string { return "//" }

// ConfigDomainPortName maps a Chinese business domain name to an English name (e.g., "认证鉴权" → "Auth")
func ConfigDomainPortName(domain string) string {
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
	return strings.ReplaceAll(domain, " ", "")
}

// PortInterfaceName returns the vtable type name (e.g., "auth_config_vtable_t")
func PortInterfaceName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "_config_vtable_t"
}

// AdapterTypeName returns the adapter struct type name (e.g., "auth_config_adapter_t")
func AdapterTypeName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "_config_adapter_t"
}

// CType maps a BlessStar type to a C type
func CType(bsType string) string {
	return types.CBlessStarType(bsType)
}

// CDefault formats a default value as a C literal
func CDefault(cType, defaultVal string) string {
	switch {
	case cType == "int64_t" || cType == "int32_t":
		if defaultVal == "" {
			return "0"
		}
		return defaultVal
	case cType == "bool":
		if defaultVal == "true" || defaultVal == "1" {
			return "true"
		}
		return "false"
	case cType == "const char*" || cType == "const char**":
		if defaultVal == "" || defaultVal == "null" {
			return "NULL"
		}
		return fmt.Sprintf("%q", defaultVal)
	default:
		if defaultVal == "" {
			return "NULL"
		}
		return fmt.Sprintf("%q", defaultVal)
	}
}

// CFuncName converts a config key (e.g., "auth.jwt.token_expiry_seconds") to a snake_case function/member name
func CFuncName(key string) string {
	parts := strings.Split(key, ".")
	if len(parts) >= 2 {
		parts = parts[1:]
	}
	return strings.Join(parts, "_")
}

// FacadeFuncName returns the facade function name for a config field (e.g., "auth_config_jwt_token_expiry_seconds")
func FacadeFuncName(domain, key string) string {
	domainLower := strings.ToLower(ConfigDomainPortName(domain))
	return domainLower + "_config_" + CFuncName(key)
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

// generateFieldDoc generates a C comment for a config field
func generateFieldDoc(field types.ConfigField, labels map[string]string) string {
	label := LabelFromKey(field.Key, labels)
	desc := field.Description
	if desc == "" {
		desc = field.AIHint
	}
	rangeHint := field.ValueRange

	var lines []string
	lines = append(lines, fmt.Sprintf(" * %s (%s)", label, field.Key))
	if desc != "" {
		lines = append(lines, fmt.Sprintf(" * 描述: %s", desc))
	}
	if rangeHint != "" {
		lines = append(lines, fmt.Sprintf(" * 建议值: %s", rangeHint))
	}
	lines = append(lines, fmt.Sprintf(" * 类型: %s", field.Type))
	return strings.Join(lines, "\n")
}

// isArrayType returns true if the BlessStar type is ARR (array)
func isArrayType(bsType string) bool {
	return bsType == "ARR"
}

// funcExtraParam returns the extra parameter declaration for array-typed functions (without leading comma)
func funcExtraParam(bsType string) string {
	if isArrayType(bsType) {
		return "size_t* out_len"
	}
	return ""
}

// funcExtraArg returns the extra argument name for array-typed function calls
func funcExtraArg(bsType string) string {
	if isArrayType(bsType) {
		return "out_len"
	}
	return ""
}

// headerGuard returns the header guard macro name for a given path
func headerGuard(bizID, suffix string) string {
	parts := []string{strings.ToUpper(strings.ReplaceAll(bizID, "-", "_")), strings.ToUpper(suffix)}
	return strings.Join(parts, "_") + "_H"
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

// registryPath returns the registry path for a config field
func registryPath(bizID string, field types.ConfigField) string {
	if field.RegistryPath != "" {
		return field.RegistryPath
	}
	return fmt.Sprintf("/config/%s/%s", bizID, strings.ReplaceAll(field.Key, ".", "/"))
}

// cIdentifier sanitizes a string to be a valid C identifier
func cIdentifier(s string) string {
	return strings.NewReplacer("-", "_", ".", "_", " ", "_").Replace(s)
}

// ---------------------------------------------------------------------------
// GeneratePortInterface — generates ports/{domain}.h with vtable + facade declarations
// ---------------------------------------------------------------------------

func (c *CBackend) GeneratePortInterface(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	domainName := ConfigDomainPortName(domain)
	domainLower := strings.ToLower(domainName)
	vtableType := PortInterfaceName(domain)
	guard := headerGuard(biz.BizID, "PORTS_"+strings.ToUpper(domainLower))

	var b strings.Builder

	// File header comment
	b.WriteString(fmt.Sprintf("/**\n"))
	b.WriteString(fmt.Sprintf(" * 自动生成于 BlessStar 配置端口 — C 头文件\n"))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf(" * 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf(" * 请勿手动修改 — 由 blessstar-codegen 自动生成\n"))
	b.WriteString(fmt.Sprintf(" */\n\n"))

	// Header guard
	b.WriteString(fmt.Sprintf("#ifndef %s\n", guard))
	b.WriteString(fmt.Sprintf("#define %s\n\n", guard))

	// Includes
	b.WriteString("#include <stdint.h>\n")
	b.WriteString("#include <stdbool.h>\n")
	b.WriteString("#include <stddef.h>\n\n")

	// C++ guard
	b.WriteString("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")

	// Domain description comment
	b.WriteString(fmt.Sprintf("/**\n * %s 域配置接口\n", domain))
	b.WriteString(fmt.Sprintf(" * 通过 vtable + setter 模式注入具体实现\n"))
	b.WriteString(fmt.Sprintf(" * 业务代码通过外观函数访问配置，无需关心底层实现\n */\n\n"))

	// Vtable typedef
	b.WriteString(fmt.Sprintf("// %s — 函数指针表\n", domainName+"Config"))
	b.WriteString(fmt.Sprintf("typedef struct {\n"))
	for _, c := range configs {
		cType := CType(c.Type)
		memberName := CFuncName(c.Key)
		extraParams := funcExtraParam(c.Type)
		implParams := "void* impl_ctx"
		if extraParams != "" {
			implParams += ", " + extraParams
		}
		b.WriteString(fmt.Sprintf("    %s (*%s)(%s);\n", cType, memberName, implParams))
	}
	b.WriteString(fmt.Sprintf("} %s;\n\n", vtableType))

	// Setter declaration
	b.WriteString(fmt.Sprintf("// 全局 setter — 由 adapter 层在初始化时调用\n"))
	b.WriteString(fmt.Sprintf("void %s_set_impl(const %s* vtable, void* impl_ctx);\n\n", domainLower+"_config", vtableType))

	// Facade function declarations
	b.WriteString(fmt.Sprintf("// 业务代码调用的外观函数\n"))
	for _, c := range configs {
		cType := CType(c.Type)
		facadeName := FacadeFuncName(domain, c.Key)
		extraParams := funcExtraParam(c.Type)
		b.WriteString(fmt.Sprintf("/**\n"))
		b.WriteString(fmt.Sprintf("%s\n", generateFieldDoc(c, biz.ConfigLabels)))
		b.WriteString(fmt.Sprintf(" */\n"))
		b.WriteString(fmt.Sprintf("%s %s(%s);\n", cType, facadeName, extraParams))
	}
	b.WriteString(fmt.Sprintf("\n"))

	// C++ guard end
	b.WriteString("#ifdef __cplusplus\n}\n#endif\n\n")

	// Header guard end
	b.WriteString(fmt.Sprintf("#endif /* %s */\n", guard))

	return &backend.File{
		Path:    fmt.Sprintf("ports/%s.h", domainLower),
		Content: b.String(),
	}, nil
}

// ---------------------------------------------------------------------------
// GenerateBlessStarAdapter — generates adapters/blessstar/{domain}_adapter.c
// ---------------------------------------------------------------------------

func (c *CBackend) GenerateBlessStarAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	domainName := ConfigDomainPortName(domain)
	domainLower := strings.ToLower(domainName)
	vtableType := PortInterfaceName(domain)
	adapterType := AdapterTypeName(domain)

	var b strings.Builder

	// File header comment
	b.WriteString(fmt.Sprintf("/**\n"))
	b.WriteString(fmt.Sprintf(" * 自动生成于 BlessStar 配置适配器 — C 实现\n"))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf(" * 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf(" * 请勿手动修改 — 由 blessstar-codegen 自动生成\n"))
	b.WriteString(fmt.Sprintf(" * 内置三阶段降级: ConfigReader → 缓存 → 硬编码默认值\n"))
	b.WriteString(fmt.Sprintf(" */\n\n"))

	// Includes
	b.WriteString(fmt.Sprintf("#include \"%s.h\"\n", domainLower))
	b.WriteString("#include \"ports/config_reader.h\"\n")
	b.WriteString("#include <stdlib.h>\n")
	b.WriteString("#include <string.h>\n\n")

	// Adapter struct
	b.WriteString(fmt.Sprintf("// %s — %s 域配置适配器结构体\n", adapterType, domain))
	b.WriteString(fmt.Sprintf("typedef struct {\n"))
	b.WriteString(fmt.Sprintf("    config_reader_t* reader;\n"))
	b.WriteString(fmt.Sprintf("    struct {\n"))
	for _, c := range configs {
		memberName := CFuncName(c.Key)
		cType := CType(c.Type)
		b.WriteString(fmt.Sprintf("        %s %s;\n", cType, memberName))
		b.WriteString(fmt.Sprintf("        bool %s_valid;\n", memberName))
		// For const char* cached values, we need to track if it needs freeing
		if cType == "const char*" || cType == "const char**" {
			b.WriteString(fmt.Sprintf("        bool %s_owned;\n", memberName))
		}
	}
	b.WriteString(fmt.Sprintf("    } cache;\n"))
	b.WriteString(fmt.Sprintf("} %s;\n\n", adapterType))

	// Static global instances
	b.WriteString(fmt.Sprintf("// 全局 vtable 指针和上下文\n"))
	b.WriteString(fmt.Sprintf("static const %s* g_%s_vtable = NULL;\n", vtableType, domainLower))
	b.WriteString(fmt.Sprintf("static void* g_%s_ctx = NULL;\n\n", domainLower))

	// ---- Helper: vtable adapter function implementations ----
	for _, c := range configs {
		memberName := CFuncName(c.Key)
		cType := CType(c.Type)
		fullExtraParams := funcExtraParam(c.Type) // "size_t* out_len" or ""
		sigExtra := ""
		if fullExtraParams != "" {
			sigExtra = ", " + fullExtraParams
		}

		b.WriteString(fmt.Sprintf("/** %s 域配置适配器 — %s */\n", domain, LabelFromKey(c.Key, biz.ConfigLabels)))
		b.WriteString(fmt.Sprintf("static %s %s_adapter_%s(void* impl_ctx%s) {\n", cType, domainLower, memberName, sigExtra))
		b.WriteString(fmt.Sprintf("    %s* adapter = (%s*)impl_ctx;\n\n", adapterType, adapterType))

		// Phase 1: ConfigReader
		regPath := registryPath(biz.BizID, c)
		b.WriteString(fmt.Sprintf("    // 第1阶段: ConfigReader 实时查询\n"))
		if isArrayType(c.Type) {
			b.WriteString(fmt.Sprintf("    if (adapter->reader) {\n"))
			b.WriteString(fmt.Sprintf("        void* val = NULL;\n"))
			b.WriteString(fmt.Sprintf("        const char* val_type = NULL;\n"))
			b.WriteString(fmt.Sprintf("        if (adapter->reader->get(adapter->reader->impl_ctx, %q, &val, &val_type) == 0 && val) {\n", regPath))
			b.WriteString(fmt.Sprintf("            const char** result = (const char**)val;\n"))
			b.WriteString(fmt.Sprintf("            if (out_len) *out_len = 0; /* caller must determine length */\n"))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s = result;\n", memberName))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s_valid = true;\n", memberName))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s_owned = false;\n", memberName))
			b.WriteString(fmt.Sprintf("            return result;\n"))
			b.WriteString(fmt.Sprintf("        }\n"))
			b.WriteString(fmt.Sprintf("    }\n\n"))
		} else if cType == "const char*" {
			b.WriteString(fmt.Sprintf("    if (adapter->reader) {\n"))
			b.WriteString(fmt.Sprintf("        void* val = NULL;\n"))
			b.WriteString(fmt.Sprintf("        const char* val_type = NULL;\n"))
			b.WriteString(fmt.Sprintf("        if (adapter->reader->get(adapter->reader->impl_ctx, %q, &val, &val_type) == 0 && val) {\n", regPath))
			b.WriteString(fmt.Sprintf("            const char* result = (const char*)val;\n"))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s = result;\n", memberName))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s_valid = true;\n", memberName))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s_owned = false;\n", memberName))
			b.WriteString(fmt.Sprintf("            return result;\n"))
			b.WriteString(fmt.Sprintf("        }\n"))
			b.WriteString(fmt.Sprintf("    }\n\n"))
		} else if cType == "bool" {
			b.WriteString(fmt.Sprintf("    if (adapter->reader) {\n"))
			b.WriteString(fmt.Sprintf("        void* val = NULL;\n"))
			b.WriteString(fmt.Sprintf("        const char* val_type = NULL;\n"))
			b.WriteString(fmt.Sprintf("        if (adapter->reader->get(adapter->reader->impl_ctx, %q, &val, &val_type) == 0 && val) {\n", regPath))
			b.WriteString(fmt.Sprintf("            bool result = *(bool*)val;\n"))
			b.WriteString(fmt.Sprintf("            free(val);\n"))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s = result;\n", memberName))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s_valid = true;\n", memberName))
			b.WriteString(fmt.Sprintf("            return result;\n"))
			b.WriteString(fmt.Sprintf("        }\n"))
			b.WriteString(fmt.Sprintf("    }\n\n"))
		} else {
			// int64_t / int32_t
			b.WriteString(fmt.Sprintf("    if (adapter->reader) {\n"))
			b.WriteString(fmt.Sprintf("        void* val = NULL;\n"))
			b.WriteString(fmt.Sprintf("        const char* val_type = NULL;\n"))
			b.WriteString(fmt.Sprintf("        if (adapter->reader->get(adapter->reader->impl_ctx, %q, &val, &val_type) == 0 && val) {\n", regPath))
			b.WriteString(fmt.Sprintf("            %s result = *(%s*)val;\n", cType, cType))
			b.WriteString(fmt.Sprintf("            free(val);\n"))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s = result;\n", memberName))
			b.WriteString(fmt.Sprintf("            adapter->cache.%s_valid = true;\n", memberName))
			b.WriteString(fmt.Sprintf("            return result;\n"))
			b.WriteString(fmt.Sprintf("        }\n"))
			b.WriteString(fmt.Sprintf("    }\n\n"))
		}

		// Phase 2: Cache
		b.WriteString(fmt.Sprintf("    // 第2阶段: 降级到缓存\n"))
		if cType == "const char*" || cType == "const char**" {
			b.WriteString(fmt.Sprintf("    if (adapter->cache.%s_valid) {\n", memberName))
			b.WriteString(fmt.Sprintf("        return adapter->cache.%s;\n", memberName))
			b.WriteString(fmt.Sprintf("    }\n\n"))
		} else if cType == "bool" {
			b.WriteString(fmt.Sprintf("    if (adapter->cache.%s_valid) {\n", memberName))
			b.WriteString(fmt.Sprintf("        return adapter->cache.%s;\n", memberName))
			b.WriteString(fmt.Sprintf("    }\n\n"))
		} else {
			b.WriteString(fmt.Sprintf("    if (adapter->cache.%s_valid) {\n", memberName))
			b.WriteString(fmt.Sprintf("        return adapter->cache.%s;\n", memberName))
			b.WriteString(fmt.Sprintf("    }\n\n"))
		}

		// Phase 3: Hardcoded default
		cTypeZero := types.CBlessStarTypeZero(cType)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = cTypeZero
		}
		b.WriteString(fmt.Sprintf("    // 第3阶段: 极冷启动 — 返回硬编码默认值\n"))
		b.WriteString(fmt.Sprintf("    return %s;\n", CDefault(cType, defaultVal)))
		b.WriteString(fmt.Sprintf("}\n\n"))
	}

	// ---- Full vtable instance ----
	b.WriteString(fmt.Sprintf("// %s 域 vtable 实例\n", domainName))
	b.WriteString(fmt.Sprintf("static const %s g_%s_vtable_impl = {\n", vtableType, domainLower))
	for _, c := range configs {
		memberName := CFuncName(c.Key)
		b.WriteString(fmt.Sprintf("    .%s = %s_adapter_%s,\n", memberName, domainLower, memberName))
	}
	b.WriteString(fmt.Sprintf("};\n\n"))

	// ---- Setter implementation ----
	b.WriteString(fmt.Sprintf("void %s_config_set_impl(const %s* vtable, void* impl_ctx) {\n", domainLower, vtableType))
	b.WriteString(fmt.Sprintf("    g_%s_vtable = vtable;\n", domainLower))
	b.WriteString(fmt.Sprintf("    g_%s_ctx = impl_ctx;\n", domainLower))
	b.WriteString(fmt.Sprintf("}\n\n"))

	// ---- Facade function implementations ----
	b.WriteString(fmt.Sprintf("// 外观函数实现 — 通过 vtable 委派实际调用\n"))
	for _, c := range configs {
		cType := CType(c.Type)
		facadeName := FacadeFuncName(domain, c.Key)
		memberName := CFuncName(c.Key)
		extraParams := funcExtraParam(c.Type)
		extraArg := funcExtraArg(c.Type)
		callExtra := ""
		if extraArg != "" {
			callExtra = ", " + extraArg
		}

		b.WriteString(fmt.Sprintf("%s %s(%s) {\n", cType, facadeName, extraParams))
		b.WriteString(fmt.Sprintf("    if (g_%s_vtable && g_%s_vtable->%s) {\n", domainLower, domainLower, memberName))
		b.WriteString(fmt.Sprintf("        return g_%s_vtable->%s(g_%s_ctx%s);\n", domainLower, memberName, domainLower, callExtra))
		b.WriteString(fmt.Sprintf("    }\n"))

		// Fallback if vtable not set
		cTypeZero := types.CBlessStarTypeZero(cType)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = cTypeZero
		}
		if isArrayType(c.Type) {
			b.WriteString(fmt.Sprintf("    if (out_len) *out_len = 0;\n"))
			b.WriteString(fmt.Sprintf("    return NULL;\n"))
		} else {
			b.WriteString(fmt.Sprintf("    return %s;\n", CDefault(cType, defaultVal)))
		}
		b.WriteString(fmt.Sprintf("}\n\n"))
	}

	return &backend.File{
		Path:    fmt.Sprintf("adapters/blessstar/%s_adapter.c", domainLower),
		Content: b.String(),
	}, nil
}

// ---------------------------------------------------------------------------
// GenerateMockAdapter — generates adapters/mock/{domain}_mock.c
// ---------------------------------------------------------------------------

func (c *CBackend) GenerateMockAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	domainLower := strings.ToLower(ConfigDomainPortName(domain))
	vtableType := PortInterfaceName(domain)

	var b strings.Builder

	// File header
	b.WriteString(fmt.Sprintf("/**\n"))
	b.WriteString(fmt.Sprintf(" * 自动生成于 BlessStar 配置 Mock — C 实现\n"))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf(" * 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf(" * 专为单元测试设计 — 固定返回值\n"))
	b.WriteString(fmt.Sprintf(" */\n\n"))

	// Includes
	b.WriteString(fmt.Sprintf("#include \"%s.h\"\n", domainLower))
	b.WriteString("#include <stdlib.h>\n\n")

	// Static mock vtable implementation functions
	for _, c := range configs {
		cType := CType(c.Type)
		memberName := CFuncName(c.Key)
		extraParams := funcExtraParam(c.Type)
		sigExtra := ""
		if extraParams != "" {
			sigExtra = ", " + extraParams
		}

		b.WriteString(fmt.Sprintf("// Mock: %s\n", LabelFromKey(c.Key, biz.ConfigLabels)))
		b.WriteString(fmt.Sprintf("static %s mock_%s(void* impl_ctx%s) {\n", cType, memberName, sigExtra))
		b.WriteString(fmt.Sprintf("    (void)impl_ctx;\n"))
		if isArrayType(c.Type) {
			b.WriteString(fmt.Sprintf("    if (out_len) *out_len = 0;\n"))
		}
		cTypeZero := types.CBlessStarTypeZero(cType)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = cTypeZero
		}
		b.WriteString(fmt.Sprintf("    return %s;\n", CDefault(cType, defaultVal)))
		b.WriteString(fmt.Sprintf("}\n\n"))
	}

	// Mock vtable
	b.WriteString(fmt.Sprintf("// Mock vtable 实例 — 返回固定值\n"))
	b.WriteString(fmt.Sprintf("static const %s g_%s_mock_vtable = {\n", vtableType, domainLower))
	for _, c := range configs {
		memberName := CFuncName(c.Key)
		b.WriteString(fmt.Sprintf("    .%s = mock_%s,\n", memberName, memberName))
	}
	b.WriteString(fmt.Sprintf("};\n\n"))

	// Public init function
	b.WriteString(fmt.Sprintf("// %s_mock_init 初始化 Mock 适配器\n", domainLower))
	b.WriteString(fmt.Sprintf("// 将全局 vtable 设置为 Mock 实现，业务代码随后调用外观函数即可获得固定值\n"))
	b.WriteString(fmt.Sprintf("void %s_mock_init(void) {\n", domainLower))
	b.WriteString(fmt.Sprintf("    %s_config_set_impl(&g_%s_mock_vtable, NULL);\n", domainLower, domainLower))
	b.WriteString(fmt.Sprintf("}\n"))

	return &backend.File{
		Path:    fmt.Sprintf("adapters/mock/%s_mock.c", domainLower),
		Content: b.String(),
	}, nil
}

// ---------------------------------------------------------------------------
// GenerateProvider — generates provider/blessstar_provider.c
// ---------------------------------------------------------------------------

func (c *CBackend) GenerateProvider(biz *types.BizSystem) (*backend.File, error) {
	var b strings.Builder

	// File header
	b.WriteString(fmt.Sprintf("/**\n"))
	b.WriteString(fmt.Sprintf(" * 自动生成于 BlessStar 配置依赖注入 — C 实现\n"))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf(" * 请勿手动修改 — 由 blessstar-codegen 自动生成\n"))
	b.WriteString(fmt.Sprintf(" */\n\n"))

	// Includes
	b.WriteString(fmt.Sprintf("#include \"ports/config_reader.h\"\n"))
	sortedDomains := sortBizDomains(biz)
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		domainLower := strings.ToLower(ConfigDomainPortName(d))
		b.WriteString(fmt.Sprintf("#include \"ports/%s.h\"\n", domainLower))
	}
	b.WriteString(fmt.Sprintf("\n"))

	// blessstar_provide_adapters function
	b.WriteString(fmt.Sprintf("/**\n"))
	b.WriteString(fmt.Sprintf(" * blessstar_provide_adapters 一行初始化所有域配置适配器。\n"))
	b.WriteString(fmt.Sprintf(" * 由业务方在启动时调用，传入 ConfigReader 实例。\n"))
	b.WriteString(fmt.Sprintf(" * reader 为 NULL 时仅加载硬编码默认值（极冷启动降级）。\n"))
	b.WriteString(fmt.Sprintf(" */\n"))
	b.WriteString(fmt.Sprintf("void %s_provide_adapters(config_reader_t* reader) {\n", cIdentifier(biz.BizID)))
	b.WriteString(fmt.Sprintf("    (void)reader;\n"))

	// Adapter struct instances and initialization per domain
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		domainLower := strings.ToLower(ConfigDomainPortName(d))
		adapterType := AdapterTypeName(d)

		b.WriteString(fmt.Sprintf("\n"))
		b.WriteString(fmt.Sprintf("    // %s 域 — 创建适配器并注册\n", d))
		b.WriteString(fmt.Sprintf("    static %s %s_adapter_inst;\n", adapterType, domainLower))
		b.WriteString(fmt.Sprintf("    %s_adapter_inst.reader = reader;\n", domainLower))
		// Zero out cache valid flags
		for _, c := range configs {
			memberName := CFuncName(c.Key)
			b.WriteString(fmt.Sprintf("    %s_adapter_inst.cache.%s_valid = false;\n", domainLower, memberName))
		}
		// Register vtable
		b.WriteString(fmt.Sprintf("    %s_config_set_impl(&g_%s_vtable_impl, &%s_adapter_inst);\n", domainLower, domainLower, domainLower))
	}
	b.WriteString(fmt.Sprintf("}\n"))

	return &backend.File{
		Path:    "provider/blessstar_provider.c",
		Content: b.String(),
	}, nil
}

// ---------------------------------------------------------------------------
// GenerateGoMod — generates CMakeLists.txt
// ---------------------------------------------------------------------------

func (c *CBackend) GenerateGoMod(biz *types.BizSystem) (*backend.File, error) {
	// Collect source files for CMake
	sortedDomains := sortBizDomains(biz)

	var srcFiles []string
	srcFiles = append(srcFiles, "provider/blessstar_provider.c")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		domainLower := strings.ToLower(ConfigDomainPortName(d))
		srcFiles = append(srcFiles, fmt.Sprintf("adapters/blessstar/%s_adapter.c", domainLower))
		srcFiles = append(srcFiles, fmt.Sprintf("adapters/mock/%s_mock.c", domainLower))
	}

	var headerFiles []string
	headerFiles = append(headerFiles, "ports/config_reader.h")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		domainLower := strings.ToLower(ConfigDomainPortName(d))
		headerFiles = append(headerFiles, fmt.Sprintf("ports/%s.h", domainLower))
	}

	// Build CMakeLists.txt content
	var cmakeSrc strings.Builder
	for i, sf := range srcFiles {
		if i > 0 {
			cmakeSrc.WriteString("\n            ")
		}
		cmakeSrc.WriteString(sf)
	}

	var cmakeHeaders strings.Builder
	for i, hf := range headerFiles {
		if i > 0 {
			cmakeHeaders.WriteString("\n            ")
		}
		cmakeHeaders.WriteString(hf)
	}

	content := fmt.Sprintf(`cmake_minimum_required(VERSION 3.10)
project(%s C)

# 自动生成于 BlessStar 配置 — 由 blessstar-codegen 生成
# 业务系统: %s (%s)

add_library(%s_config STATIC
            %s)

target_include_directories(%s_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

`, cIdentifier(biz.BizID), biz.DisplayName, biz.BizID, cIdentifier(biz.BizID), cmakeSrc.String(), cIdentifier(biz.BizID))

	return &backend.File{
		Path:    "CMakeLists.txt",
		Content: content,
	}, nil
}

// ---------------------------------------------------------------------------
// GenerateConfigReaderFile — generates ports/config_reader.h
// ---------------------------------------------------------------------------

func (c *CBackend) GenerateConfigReaderFile(biz *types.BizSystem) ([]*backend.File, error) {
	// ports/config_reader.h
	configReaderH := fmt.Sprintf(`/**
 * 自动生成于 BlessStar 配置 — ConfigReader C 头文件
 * 业务系统: %s (%s)
 * 请勿手动修改 — 由 blessstar-codegen 自动生成
 */

#ifndef PORTS_CONFIG_READER_H
#define PORTS_CONFIG_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * config_reader_t — 配置读取器接口
 * 通过 vtable + impl_ctx 实现多态
 * 内置实现包括: CachedReader（秒级轮询缓存）、EnvReader（环境变量）、FileReader（本地文件）等。
 *
 * get: 读取一个配置值。
 *   @param ctx    实现上下文指针
 *   @param path   配置的完整注册路径 (如 "/config/%[2]s/auth/jwt/token_expiry_seconds")
 *   @param out_val 输出配置值 (需要 free 释放)
 *   @param out_type 输出类型字符串 (如 "int64_t", "bool", "const char*")
 *   @return 0 成功, 非0 失败
 */
typedef struct {
    void* impl_ctx;
    int (*get)(void* ctx, const char* path, void** out_val, const char** out_type);
} config_reader_vtable_t;

// config_reader_t 结构体 — 业务方使用的读取器句柄
typedef struct {
    const config_reader_vtable_t* vtable;
} config_reader_t;

// 全局 ConfigReader 实例
extern config_reader_t* g_config_reader;

// 设置全局 ConfigReader
void config_reader_set_impl(const config_reader_vtable_t* vtable);

#ifdef __cplusplus
}
#endif

#endif /* PORTS_CONFIG_READER_H */
`, biz.DisplayName, biz.BizID)

	// provider/cached_reader.c — optional CachedReader implementation
	cachedReaderC := fmt.Sprintf(`/**
 * 自动生成于 BlessStar 配置 — CachedReader C 实现
 * 请勿手动修改 — 由 blessstar-codegen 自动生成
 */

#include "ports/config_reader.h"
#include <stdlib.h>
#include <string.h>

// 简单的缓存条目
typedef struct cache_entry {
    char* path;
    void* value;
    char* type;
    struct cache_entry* next;
} cache_entry_t;

// CachedReader 结构体
typedef struct {
    config_reader_vtable_t vtable;
    config_reader_vtable_t* inner;  // 内部读取器的 vtable
    cache_entry_t* head;
} cached_reader_t;

// 内部 get 实现
static int cached_reader_get(void* ctx, const char* path, void** out_val, const char** out_type) {
    cached_reader_t* cr = (cached_reader_t*)ctx;
    
    // 先查缓存
    cache_entry_t* entry = cr->head;
    while (entry) {
        if (strcmp(entry->path, path) == 0) {
            if (entry->value) {
                // 返回缓存值的拷贝
                // NOTE: 简化实现，实际场景需要根据 type 做深拷贝
                *out_val = entry->value;
                *out_type = entry->type;
                return 0;
            }
            break;
        }
        entry = entry->next;
    }
    
    // 穿透到内部读取器
    if (cr->inner && cr->inner->get) {
        int ret = cr->inner->get(cr->inner->impl_ctx, path, out_val, out_type);
        if (ret == 0 && *out_val) {
            // 写入缓存
            cache_entry_t* new_entry = (cache_entry_t*)malloc(sizeof(cache_entry_t));
            if (new_entry) {
                new_entry->path = (char*)malloc(strlen(path) + 1);
                if (new_entry->path) strcpy(new_entry->path, path);
                new_entry->value = *out_val;  // NOTE: 简化实现，未做深拷贝
                new_entry->type = *out_type ? strdup(*out_type) : NULL;
                new_entry->next = cr->head;
                cr->head = new_entry;
            }
        }
        return ret;
    }
    
    return -1;
}

// 创建 CachedReader
config_reader_t* cached_reader_create(config_reader_vtable_t* inner_vtable) {
    cached_reader_t* cr = (cached_reader_t*)malloc(sizeof(cached_reader_t));
    if (!cr) return NULL;
    
    cr->vtable.get = cached_reader_get;
    cr->inner = inner_vtable;
    cr->head = NULL;
    
    config_reader_t* reader = (config_reader_t*)malloc(sizeof(config_reader_t));
    if (!reader) {
        free(cr);
        return NULL;
    }
    reader->vtable = &cr->vtable;
    
    // 注册到全局
    config_reader_set_impl(&cr->vtable);
    
    return reader;
}
`)

	return []*backend.File{
		{
			Path:    "ports/config_reader.h",
			Content: configReaderH,
		},
		{
			Path:    "provider/cached_reader.c",
			Content: cachedReaderC,
		},
	}, nil
}

// Ensure interface compliance
var _ backend.LanguageBackend = (*CBackend)(nil)
