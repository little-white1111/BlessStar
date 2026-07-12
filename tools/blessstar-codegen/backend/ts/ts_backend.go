package ts_backend

import (
	"fmt"
	"sort"
	"strings"

	"github.com/blessstar/blessstar-codegen/backend"
	"github.com/blessstar/blessstar-codegen/types"
)

// TSBackend implements the LanguageBackend for TypeScript
type TSBackend struct{}

func New() *TSBackend { return &TSBackend{} }

func (g *TSBackend) Name() string          { return "ts" }
func (g *TSBackend) FileExtension() string { return ".ts" }
func (g *TSBackend) CommentPrefix() string { return "//" }

// ConfigDomainPortName maps a config domain name to a TypeScript port interface name
func ConfigDomainPortName(domain string) string {
	domainMap := map[string]string{
		"livedesign": "LiveDesign",
		"avatar":     "Avatar",
		"chat":       "Chat",
		"assistant":  "Assistant",
		"ui":         "UI",
		"plugin":     "Plugin",
	}
	if name, ok := domainMap[domain]; ok {
		return name
	}
	// Fallback: PascalCase the domain name
	return strings.ReplaceAll(strings.Title(domain), " ", "")
}

// PortInterfaceName returns the TypeScript interface name
func PortInterfaceName(domain string) string {
	return ConfigDomainPortName(domain) + "Config"
}

// PortFileName returns the file name for the port
func PortFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + ".ts"
}

// AdapterTypeName returns the TypeScript class name for the BlessStar adapter
func AdapterTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigAdapter"
}

// MockTypeName returns the TypeScript class name for the mock adapter
func MockTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigMock"
}

// AdapterFileName returns the file name for the adapter
func AdapterFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "-adapter.ts"
}

// MockFileName returns the file name for the mock
func MockFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "-mock.ts"
}

// TSType maps a BlessStar type to TypeScript
func TSType(bsType string) string {
	switch bsType {
	case "I64":
		return "number"
	case "I32":
		return "number"
	case "F64":
		return "number"
	case "STR":
		return "string"
	case "BOOL":
		return "boolean"
	case "ARR":
		return "string[]"
	case "ENUM":
		return "string"
	default:
		return "string"
	}
}

// toPascalCase converts a dot-separated or snake_case string to PascalCase
func toPascalCase(s string) string {
	parts := strings.FieldsFunc(s, func(r rune) bool {
		return r == '_' || r == '.'
	})
	for i, p := range parts {
		if len(p) > 0 {
			parts[i] = strings.ToUpper(p[:1]) + p[1:]
		}
	}
	return strings.Join(parts, "")
}

// toCamelCase converts a dot-separated or snake_case string to camelCase
func toCamelCase(s string) string {
	parts := strings.FieldsFunc(s, func(r rune) bool {
		return r == '_' || r == '.'
	})
	for i, p := range parts {
		if i == 0 && len(p) > 0 {
			parts[i] = strings.ToLower(p[:1]) + p[1:]
		} else if len(p) > 0 {
			parts[i] = strings.ToUpper(p[:1]) + p[1:]
		}
	}
	return strings.Join(parts, "")
}

// MethodNameFromKey maps a config key to a TypeScript method name (camelCase)
func MethodNameFromKey(key string) string {
	return toCamelCase(key)
}

// LabelFromKey returns the config label
func LabelFromKey(key string, labels map[string]string) string {
	if label, ok := labels[key]; ok && label != "" {
		return label
	}
	parts := strings.Split(key, ".")
	lastPart := strings.ReplaceAll(parts[len(parts)-1], "_", " ")
	if len(lastPart) > 0 {
		lastPart = strings.ToUpper(lastPart[:1]) + lastPart[1:]
	}
	return lastPart
}

// generateFieldDoc generates TS comment for a config field
func generateFieldDoc(field types.ConfigField, labels map[string]string) string {
	label := LabelFromKey(field.Key, labels)
	desc := field.Description
	if desc == "" {
		desc = field.AIHint
	}

	var lines []string
	lines = append(lines, fmt.Sprintf(" * %s (%s)", label, field.Key))
	if desc != "" {
		lines = append(lines, fmt.Sprintf(" * 描述: %s", desc))
	}
	if field.ValueRange != "" {
		lines = append(lines, fmt.Sprintf(" * 建议值: %s", field.ValueRange))
	}
	lines = append(lines, fmt.Sprintf(" * 类型: %s", field.Type))
	return strings.Join(lines, "\n")
}

func (g *TSBackend) GeneratePortInterface(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)

	var b strings.Builder
	b.WriteString("// Package ports 自动生成于 BlessStar 配置端口-适配器\n")
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("// 领域: %s\n", domain))
	b.WriteString("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n")

	b.WriteString("/**\n")
	b.WriteString(fmt.Sprintf(" * %s %s 域配置接口\n", interfaceName, domain))
	b.WriteString(fmt.Sprintf(" * 对应 domain: %q\n", domain))
	b.WriteString(" * 禁止直接 import 配置存储 — 请通过此接口访问配置\n")
	b.WriteString(" */\n")
	b.WriteString(fmt.Sprintf("export interface %s {\n", interfaceName))

	for _, c := range configs {
		tsType := TSType(c.Type)
		methodName := MethodNameFromKey(c.Key)

		b.WriteString("\n")
		b.WriteString("  /**\n")
		b.WriteString(fmt.Sprintf("%s\n", generateFieldDoc(c, biz.ConfigLabels)))
		b.WriteString(fmt.Sprintf("   * @returns {Promise<%s>}\n", tsType))
		b.WriteString("   */\n")
		b.WriteString(fmt.Sprintf("  %s(): Promise<%s>;\n", methodName, tsType))
	}

	b.WriteString("}\n")

	return &backend.File{
		Path:    fmt.Sprintf("ports/%s", PortFileName(domain)),
		Content: b.String(),
	}, nil
}

func (g *TSBackend) GenerateBlessStarAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	adapterName := AdapterTypeName(domain)

	var b strings.Builder
	b.WriteString("// Package adapters/blessstar 自动生成于 BlessStar 配置端口-适配器\n")
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("// 领域: %s\n", domain))
	b.WriteString("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n")

	b.WriteString("import { ConfigReader } from '../../ports/config-reader';\n")
	b.WriteString(fmt.Sprintf("import { %s } from '../../ports/%s';\n\n", interfaceName, strings.ToLower(ConfigDomainPortName(domain))))

	b.WriteString("/**\n")
	b.WriteString(fmt.Sprintf(" * %s %s 域配置的 BlessStar 适配器\n", adapterName, domain))
	b.WriteString(" * 内置三阶段降级: ConfigReader实时查询 → LastKnownGood缓存 → 硬编码默认值\n")
	b.WriteString(" */\n")
	b.WriteString(fmt.Sprintf("export class %s implements %s {\n", adapterName, interfaceName))
	b.WriteString("  private reader: ConfigReader;\n")
	b.WriteString("  private lastKnownCache: Map<string, unknown> = new Map();\n")
	b.WriteString("  private hardcodedDefaults: Record<string, unknown>;\n\n")

	// Constructor
	b.WriteString("  /**\n")
	b.WriteString(fmt.Sprintf("   * @param reader 配置读取器，由业务方注入（可为 CachedReader、HTTPReader 等实现）\n"))
	b.WriteString("   */\n")
	b.WriteString(fmt.Sprintf("  constructor(reader: ConfigReader) {\n"))
	b.WriteString("    this.reader = reader;\n")
	b.WriteString("    this.hardcodedDefaults = {\n")
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = TSTypeZero(TSType(c.Type))
		}
		b.WriteString(fmt.Sprintf("      %s: %s,\n", methodName, formatTSDefault(TSType(c.Type), defaultVal)))
	}
	b.WriteString("    };\n")
	b.WriteString("  }\n\n")

	// Method implementations with 3-stage degradation
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		tsType := TSType(c.Type)

		registryPath := c.RegistryPath
		if registryPath == "" {
			registryPath = fmt.Sprintf("/config/%s/%s", biz.BizID, strings.ReplaceAll(c.Key, ".", "/"))
		}

		b.WriteString("  /**\n")
		b.WriteString(fmt.Sprintf("%s\n", generateFieldDoc(c, biz.ConfigLabels)))
		b.WriteString("   */\n")
		b.WriteString(fmt.Sprintf("  async %s(): Promise<%s> {\n", methodName, tsType))

		// Stage 1: ConfigReader
		b.WriteString(fmt.Sprintf("    // 第1阶段: ConfigReader 实时查询\n"))
		b.WriteString(fmt.Sprintf("    try {\n"))
		b.WriteString(fmt.Sprintf("      const val = await this.reader.get(%q);\n", registryPath))
		b.WriteString(fmt.Sprintf("      if (val !== undefined && val !== null) {\n"))
		b.WriteString(fmt.Sprintf("        this.lastKnownCache.set(%q, val);\n", methodName))
		b.WriteString(fmt.Sprintf("        return val as %s;\n", tsType))
		b.WriteString("      }\n")
		b.WriteString("    } catch {\n")
		b.WriteString("      // 降级到下一阶段\n")
		b.WriteString("    }\n\n")

		// Stage 2: Last known good cache
		b.WriteString(fmt.Sprintf("    // 第2阶段: 降级到 Last Known Good 缓存\n"))
		b.WriteString(fmt.Sprintf("    if (this.lastKnownCache.has(%q)) {\n", methodName))
		b.WriteString(fmt.Sprintf("      return this.lastKnownCache.get(%q) as %s;\n", methodName, tsType))
		b.WriteString("    }\n\n")

		// Stage 3: Hardcoded default
		b.WriteString(fmt.Sprintf("    // 第3阶段: 极冷启动 — 返回硬编码默认值\n"))
		b.WriteString(fmt.Sprintf("    return this.hardcodedDefaults[%q] as %s;\n", methodName, tsType))
		b.WriteString("  }\n\n")
	}

	b.WriteString("}\n")

	return &backend.File{
		Path:    fmt.Sprintf("adapters/blessstar/%s", AdapterFileName(domain)),
		Content: b.String(),
	}, nil
}

func (g *TSBackend) GenerateMockAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	mockName := MockTypeName(domain)

	var b strings.Builder
	b.WriteString("// Package adapters/mock 自动生成于 BlessStar 配置 Mock\n")
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("// 领域: %s\n", domain))
	b.WriteString("// 专为单元测试设计 — 固定返回值\n\n")

	b.WriteString(fmt.Sprintf("import { %s } from '../../ports/%s';\n\n", interfaceName, strings.ToLower(ConfigDomainPortName(domain))))

	b.WriteString("/**\n")
	b.WriteString(fmt.Sprintf(" * %s %s 域配置的 Mock 实现（单元测试用）\n", mockName, domain))
	b.WriteString(" */\n")
	b.WriteString(fmt.Sprintf("export class %s implements %s {\n", mockName, interfaceName))

	// Method implementations with fixed defaults
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		tsType := TSType(c.Type)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = TSTypeZero(tsType)
		}

		b.WriteString("\n")
		b.WriteString("  /**\n")
		b.WriteString(fmt.Sprintf("%s\n", generateFieldDoc(c, biz.ConfigLabels)))
		b.WriteString("   */\n")
		b.WriteString(fmt.Sprintf("  async %s(): Promise<%s> {\n", methodName, tsType))
		b.WriteString(fmt.Sprintf("    return %s;\n", formatTSDefault(tsType, defaultVal)))
		b.WriteString("  }\n")
	}

	b.WriteString("}\n")

	return &backend.File{
		Path:    fmt.Sprintf("adapters/mock/%s", MockFileName(domain)),
		Content: b.String(),
	}, nil
}

func (g *TSBackend) GenerateProvider(biz *types.BizSystem) (*backend.File, error) {
	sortedDomains := sortBizDomains(biz)

	var b strings.Builder
	b.WriteString("// Package provider 自动生成于 BlessStar 配置依赖注入\n")
	b.WriteString(fmt.Sprintf("// 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n")

	b.WriteString("import { ConfigReader } from '../ports/config-reader';\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		adapterName := AdapterTypeName(d)
		portFile := strings.ToLower(ConfigDomainPortName(d))
		b.WriteString(fmt.Sprintf("import { %s } from '../ports/%s';\n", portName, portFile))
		b.WriteString(fmt.Sprintf("import { %s } from '../adapters/blessstar/%s';\n", adapterName, strings.ToLower(ConfigDomainPortName(d))+"-adapter"))
	}
	b.WriteString("\n")

	b.WriteString("/**\n")
	b.WriteString(" * Adapters 持有所有域的配置适配器\n")
	b.WriteString(" */\n")
	b.WriteString("export class Adapters {\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		b.WriteString(fmt.Sprintf("  readonly %s: %s;\n", toCamelCase(portName), portName))
	}
	b.WriteString("\n")
	b.WriteString("  constructor(reader: ConfigReader) {\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		adapterName := AdapterTypeName(d)
		b.WriteString(fmt.Sprintf("    this.%s = new %s(reader);\n", toCamelCase(portName), adapterName))
	}
	b.WriteString("  }\n")
	b.WriteString("}\n\n")

	// Provide functions
	b.WriteString("/**\n")
	b.WriteString(" * provideBlessStarAdapters 一行实例化所有域配置适配器\n")
	b.WriteString(" * 注入 ConfigReader 后返回所有域的 Port 实现\n")
	b.WriteString(" * reader 由业务方提供（可为 CachedReader、HTTPReader、EnvReader 等实现）\n")
	b.WriteString(" */\n")
	b.WriteString("export function provideBlessStarAdapters(reader: ConfigReader): Adapters {\n")
	b.WriteString("  return new Adapters(reader);\n")
	b.WriteString("}\n")

	return &backend.File{
		Path:    "provider/index.ts",
		Content: b.String(),
	}, nil
}

func (g *TSBackend) GenerateGoMod(biz *types.BizSystem) (*backend.File, error) {
	// TypeScript doesn't use go.mod; generate tsconfig.json instead
	content := fmt.Sprintf(`// TypeScript Port-Adapter 自动生成配置
// 业务系统: %[1]s (%[2]s)
// 请勿手动修改 — 由 blessstar-codegen 自动生成

{
  "compilerOptions": {
    "target": "ES2022",
    "module": "commonjs",
    "strict": true,
    "esModuleInterop": true,
    "declaration": true,
    "outDir": "./dist",
    "rootDir": ".",
    "resolveJsonModule": true,
    "skipLibCheck": true
  },
  "include": ["ports/**/*", "adapters/**/*", "provider/**/*"]
}
`, biz.DisplayName, biz.BizID)

	return &backend.File{
		Path:    "tsconfig.json",
		Content: content,
	}, nil
}

func (g *TSBackend) GenerateConfigReaderFile(biz *types.BizSystem) ([]*backend.File, error) {
	pkgName := biz.BizID

	// Generate ports/config-reader.ts — the ConfigReader interface
	configReaderContent := fmt.Sprintf(`// Package ports 提供业务系统的配置读取接口。
// 所有 Port 接口定义在此包中，业务代码仅依赖此包。
// 请勿手动修改 — 由 blessstar-codegen 自动生成

/**
 * ConfigReader 是配置读取接口，业务方自行选择实现方式。
 * 内置实现包括：CachedReader（秒级轮询缓存）、EnvReader（环境变量）、FileReader（本地文件）等。
 *
 * 实现类通过环境变量 BLESSSTAR_ENDPOINT 获取 Electron 地址（HTTPReader 场景）。
 * 初始化失败时不得阻塞进程，由 adapter 的三阶段降级兜底。
 *
 * get 返回 unknown 以便 adapter 进行类型断言（与三阶段降级兼容）。
 * 常见返回类型：number, boolean, string, string[]。
 */
export interface ConfigReader {
  /**
   * 读取一个配置值。
   * @param path 配置的完整注册路径（如 "/config/%[1]s/avatar/position_x"）
   * @returns 配置值（可直接类型断言）或 undefined
   */
  get(path: string): Promise<unknown>;
}
`, pkgName)

	// Generate provider/cached-reader.ts — optional CachedReader decorator
	cachedReaderContent := `// Package provider 提供 BlessStar 配置的依赖注入和可选装饰器。
// 请勿手动修改 — 由 blessstar-codegen 自动生成

import { ConfigReader } from '../ports/config-reader';

/**
 * CachedReader 是对 ConfigReader 的缓存包装器（可选装饰器）。
 * 使用 setInterval 定时刷新缓存，实现秒级准实时热更新。
 * 不依赖任何外部库。
 *
 * 使用方式（由 main.ts 注入）：
 *
 *   const rawReader: ConfigReader = new HttpReader();
 *   const cachedReader = new CachedReader(rawReader, 30_000);
 *   const adapters = provideBlessStarAdapters(cachedReader);
 */
export class CachedReader implements ConfigReader {
  private inner: ConfigReader;
  private cache: Map<string, unknown> = new Map();
  private intervalId: ReturnType<typeof setInterval> | null = null;
  private refreshFn: () => Promise<void> = async () => {};

  /**
   * @param inner 底层 ConfigReader 实现
   * @param intervalMs 缓存刷新周期（毫秒）
   */
  constructor(inner: ConfigReader, intervalMs: number = 30000) {
    this.inner = inner;
    this.intervalId = setInterval(async () => {
      try {
        await this.refreshFn();
      } catch {
        // 刷新失败不影响现有缓存，仅跳过本轮
      }
    }, intervalMs);
  }

  /**
   * setRefreshFn 设置全量刷新回调函数，由业务方自定义需要缓存的配置路径。
   */
  setRefreshFn(fn: () => Promise<void>): this {
    this.refreshFn = fn;
    return this;
  }

  /**
   * 从缓存中读取配置值。若缓存命中直接返回，否则穿透到 inner ConfigReader。
   */
  async get(path: string): Promise<unknown> {
    if (this.cache.has(path)) {
      return this.cache.get(path);
    }
    const val = await this.inner.get(path);
    if (val !== undefined) {
      this.cache.set(path, val);
    }
    return val;
  }

  /**
   * 手动设置缓存值（用于预热）。
   */
  set(path: string, value: unknown): void {
    this.cache.set(path, value);
  }

  /**
   * 停止后台刷新定时器。
   */
  destroy(): void {
    if (this.intervalId !== null) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
    this.cache.clear();
  }
}`

	return []*backend.File{
		{
			Path:    "ports/config-reader.ts",
			Content: configReaderContent,
		},
		{
			Path:    "provider/cached-reader.ts",
			Content: cachedReaderContent,
		},
	}, nil
}

func (g *TSBackend) GenerateInitFiles(biz *types.BizSystem) ([]*backend.File, error) {
	return nil, nil
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

// TSTypeZero returns the zero value literal for a TypeScript type
func TSTypeZero(tsType string) string {
	switch tsType {
	case "number":
		return "0"
	case "boolean":
		return "false"
	case "string":
		return `""`
	case "string[]":
		return "[]"
	default:
		return "undefined"
	}
}

// formatTSDefault formats a default value as a TypeScript literal
func formatTSDefault(tsType, defaultVal string) string {
	switch tsType {
	case "number":
		if defaultVal == "" {
			return "0"
		}
		return defaultVal
	case "boolean":
		if defaultVal == "true" || defaultVal == "1" {
			return "true"
		}
		return "false"
	case "string":
		if defaultVal == "" {
			return `""`
		}
		return fmt.Sprintf("%q", defaultVal)
	case "string[]":
		// Parse JSON array like '["code_edit", "file_browse"]' → ['code_edit', 'file_browse']
		if defaultVal == "" || defaultVal == "null" || defaultVal == "[]" {
			return "[]"
		}
		trimmed := strings.TrimSpace(defaultVal)
		trimmed = strings.TrimPrefix(trimmed, "[")
		trimmed = strings.TrimSuffix(trimmed, "]")
		if trimmed == "" {
			return "[]"
		}
		parts := strings.Split(trimmed, ",")
		var sb strings.Builder
		sb.WriteString("[")
		for i, p := range parts {
			if i > 0 {
				sb.WriteString(", ")
			}
			elem := strings.TrimSpace(p)
			elem = strings.Trim(elem, `"`)
			sb.WriteString(fmt.Sprintf("%q", elem))
		}
		sb.WriteString("]")
		return sb.String()
	default:
		if defaultVal == "" {
			return "undefined"
		}
		return fmt.Sprintf("%q", defaultVal)
	}
}

// ─── Schema-First 扩展方法 ───

func (g *TSBackend) GenerateGateConfigs(biz *types.BizSystem, gateConfigs []types.GateConfig) (*backend.File, error) {
	if len(gateConfigs) == 0 {
		return nil, nil
	}

	// Group by field key
	type gateEntry struct {
		GateType string
		ParamKey string
		ParamVal string
	}
	type fieldGates struct {
		FieldKey string
		Gates    []gateEntry
	}
	fieldMap := make(map[string][]gateEntry)
	for _, g := range gateConfigs {
		fieldMap[g.FieldKey] = append(fieldMap[g.FieldKey], gateEntry{
			GateType: g.GateType,
			ParamKey: g.ParamKey,
			ParamVal: g.ParamVal,
		})
	}

	var fieldKeys []string
	for k := range fieldMap {
		fieldKeys = append(fieldKeys, k)
	}
	sort.Strings(fieldKeys)

	var b strings.Builder
	b.WriteString("// Gate configs for config-schema.yaml contracts\n")
	b.WriteString("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n")

	b.WriteString("/**\n")
	b.WriteString(" * GateRule 门禁规则定义\n")
	b.WriteString(" */\n")
	b.WriteString("export interface GateRule {\n")
	b.WriteString("  fieldKey: string;\n")
	b.WriteString("  gateType: 'RANGE' | 'DEPENDENCY' | 'APPROVAL' | 'SLO_WARNING';\n")
	b.WriteString("  params: Record<string, string>;\n")
	b.WriteString("}\n\n")

	b.WriteString("/**\n")
	b.WriteString(" * GATE_RULES 从 config-schema.yaml contract 段自动生成的门禁规则\n")
	b.WriteString(" */\n")
	b.WriteString("export const GATE_RULES: GateRule[] = [\n")
	for _, fk := range fieldKeys {
		entries := fieldMap[fk]
		for _, e := range entries {
			b.WriteString(fmt.Sprintf("  {\n"))
			b.WriteString(fmt.Sprintf("    fieldKey: %q,\n", fk))
			b.WriteString(fmt.Sprintf("    gateType: %q,\n", e.GateType))
			b.WriteString(fmt.Sprintf("    params: { %s: %q },\n", e.ParamKey, e.ParamVal))
			b.WriteString("  },\n")
		}
	}
	b.WriteString("];\n")

	return &backend.File{
		Path:    "gate-configs.ts",
		Content: b.String(),
	}, nil
}

func (g *TSBackend) GenerateTestCases(biz *types.BizSystem, testCases []string) (*backend.File, error) {
	if len(testCases) == 0 {
		return nil, nil
	}

	var b strings.Builder
	b.WriteString("// Code generated by blessstar-codegen. DO NOT EDIT.\n")
	b.WriteString("// Source: config-schema.yaml\n")
	b.WriteString(fmt.Sprintf("// Business System: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString("//\n")
	b.WriteString("// config-boundary.test.ts — 配置边界测试用例\n")
	b.WriteString("// 由 config-schema.yaml contract 段（range + dependencies）自动生成\n\n")

	b.WriteString("import { describe, it, expect } from 'vitest';\n\n")

	for _, tc := range testCases {
		b.WriteString(tc)
		b.WriteString("\n")
	}

	return &backend.File{
		Path:    "tests/config-boundary.test.ts",
		Content: b.String(),
	}, nil
}

func (g *TSBackend) GenerateObservability(biz *types.BizSystem, rules []string) (*backend.File, error) {
	// Observability for TypeScript: generate a simple health-check config
	if len(rules) == 0 {
		return nil, nil
	}

	var b strings.Builder
	b.WriteString("// Observability rules for config-schema.yaml contracts\n")
	b.WriteString("// 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n")

	b.WriteString("/**\n")
	b.WriteString(" * ConfigSLO 配置 SLO 定义\n")
	b.WriteString(" */\n")
	b.WriteString("export interface ConfigSLO {\n")
	b.WriteString("  configKey: string;\n")
	b.WriteString("  sloImpact: string;\n")
	b.WriteString("  alertThreshold: number;\n")
	b.WriteString("}\n\n")

	b.WriteString("/**\n")
	b.WriteString(" * CONFIG_SLOS 从 config-schema.yaml contract.slo_impact 自动生成\n")
	b.WriteString(" */\n")
	b.WriteString("export const CONFIG_SLOS: ConfigSLO[] = [\n")
	for _, rule := range rules {
		b.WriteString(fmt.Sprintf("  %s,\n", rule))
	}
	b.WriteString("];\n")

	return &backend.File{
		Path:    "observability/config-slos.ts",
		Content: b.String(),
	}, nil
}

// Ensure interface satisfaction
var _ backend.LanguageBackend = (*TSBackend)(nil)
