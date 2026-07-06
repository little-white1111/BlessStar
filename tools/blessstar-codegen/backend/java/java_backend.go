package java_backend

import (
	"fmt"
	"sort"
	"strings"

	"github.com/blessstar/blessstar-codegen/backend"
	"github.com/blessstar/blessstar-codegen/types"
)

// JavaBackend implements the LanguageBackend for Java
type JavaBackend struct{}

func New() *JavaBackend { return &JavaBackend{} }

func (j *JavaBackend) Name() string          { return "java" }
func (j *JavaBackend) FileExtension() string { return ".java" }
func (j *JavaBackend) CommentPrefix() string { return "//" }

// ConfigDomainPortName maps a Chinese business domain name to a Java port interface name
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

// PortInterfaceName returns the full Java interface name
func PortInterfaceName(domain string) string {
	return ConfigDomainPortName(domain) + "Config"
}

// PortFileName returns the file name for the port (e.g., AuthConfig.java)
func PortFileName(domain string) string {
	return PortInterfaceName(domain) + ".java"
}

// AdapterTypeName returns the Java class name for the BlessStar adapter
func AdapterTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigAdapter"
}

// AdapterFileName returns the file name for the adapter
func AdapterFileName(domain string) string {
	return AdapterTypeName(domain) + ".java"
}

// MockTypeName returns the Java class name for the mock adapter
func MockTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigMock"
}

// MockFileName returns the file name for the mock
func MockFileName(domain string) string {
	return MockTypeName(domain) + ".java"
}

// JavaPackageFromBiz converts a biz_id like "douyin-mall" to a Java package like "com.douyinmall"
func JavaPackageFromBiz(bizID string) string {
	s := strings.ReplaceAll(bizID, "-", "")
	return "com." + s
}

// JavaType maps a BlessStar type to Java
func JavaType(bsType string) string {
	return types.JavaBlessStarType(bsType)
}

// toCamelCase converts a snake_case or dot-separated string to CamelCase.
// Examples: "token_expiry_seconds" -> "TokenExpirySeconds", "jwt" -> "Jwt"
func toCamelCase(s string) string {
	parts := strings.FieldsFunc(s, func(r rune) bool {
		return r == '_' || r == '.'
	})
	for i, p := range parts {
		if len(p) > 0 {
			parts[i] = string(p[0]-32) + p[1:]
		}
	}
	return strings.Join(parts, "")
}

// MethodNameFromKey maps a config key to a Java getter-style method name
// "auth.jwt.token_expiry_seconds" -> "getJwtTokenExpirySeconds"
func MethodNameFromKey(key string) string {
	parts := strings.Split(key, ".")
	if len(parts) >= 2 {
		parts = parts[1:]
	}
	var camelParts []string
	for _, p := range parts {
		camelParts = append(camelParts, toCamelCase(p))
	}
	return "get" + strings.Join(camelParts, "")
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

// generateFieldDoc generates Java Javadoc comment for a config field
func generateFieldDoc(field types.ConfigField, labels map[string]string) string {
	label := LabelFromKey(field.Key, labels)
	desc := field.Description
	if desc == "" {
		desc = field.AIHint
	}
	rangeHint := field.ValueRange

	var lines []string
	lines = append(lines, fmt.Sprintf("\t/**"))
	lines = append(lines, fmt.Sprintf("\t * %s (%s)", label, field.Key))
	if desc != "" {
		lines = append(lines, fmt.Sprintf("\t * 描述: %s", desc))
	}
	if rangeHint != "" {
		lines = append(lines, fmt.Sprintf("\t * 建议值: %s", rangeHint))
	}
	lines = append(lines, fmt.Sprintf("\t * 类型: %s", field.Type))
	lines = append(lines, fmt.Sprintf("\t */"))
	return strings.Join(lines, "\n")
}

// formatJavaDefault formats a default value as a Java literal
func formatJavaDefault(javaType, defaultVal string) string {
	switch {
	case javaType == "long":
		if defaultVal == "" {
			return "0L"
		}
		return defaultVal + "L"
	case javaType == "int":
		if defaultVal == "" {
			return "0"
		}
		return defaultVal
	case javaType == "boolean":
		if defaultVal == "true" || defaultVal == "1" {
			return "true"
		}
		return "false"
	case javaType == "String":
		if defaultVal == "" {
			return `""`
		}
		return fmt.Sprintf("%q", defaultVal)
	case javaType == "java.util.List<String>":
		if defaultVal == "" || defaultVal == "null" {
			return "null"
		}
		trimmed := strings.TrimSpace(defaultVal)
		trimmed = strings.TrimPrefix(trimmed, "[")
		trimmed = strings.TrimSuffix(trimmed, "]")
		if trimmed == "" {
			return "null"
		}
		parts := strings.Split(trimmed, ",")
		var sb strings.Builder
		sb.WriteString("java.util.List.of(")
		for i, p := range parts {
			if i > 0 {
				sb.WriteString(", ")
			}
			elem := strings.TrimSpace(p)
			elem = strings.Trim(elem, `"`)
			sb.WriteString(fmt.Sprintf("%q", elem))
		}
		sb.WriteString(")")
		return sb.String()
	default:
		if defaultVal == "" {
			return `""`
		}
		return fmt.Sprintf("%q", defaultVal)
	}
}

// javaSourcePath returns the src/main/java path prefix for the biz package
func javaSourcePath(bizID string) string {
	pkg := JavaPackageFromBiz(bizID)
	return fmt.Sprintf("src/main/java/%s", strings.ReplaceAll(pkg, ".", "/"))
}

// GeneratePortInterface generates a Java interface for the given domain
func (j *JavaBackend) GeneratePortInterface(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	pkg := JavaPackageFromBiz(biz.BizID)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("package %s.ports;\n\n", pkg))
	b.WriteString("/**\n")
	b.WriteString(fmt.Sprintf(" * %s域配置接口\n", domain))
	b.WriteString(fmt.Sprintf(" * 自动生成于 BlessStar 配置端口-适配器\n"))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf(" * 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf(" * 请勿手动修改 — 由 blessstar-codegen 自动生成\n"))
	b.WriteString(" */\n")
	b.WriteString(fmt.Sprintf("public interface %s {\n\n", interfaceName))

	for _, c := range configs {
		javaType := JavaType(c.Type)
		methodName := MethodNameFromKey(c.Key)

		b.WriteString(generateFieldDoc(c, biz.ConfigLabels))
		b.WriteString("\n")
		b.WriteString(fmt.Sprintf("\t%s %s() throws Exception;\n\n", javaType, methodName))
	}

	b.WriteString("}\n")

	dir := fmt.Sprintf("%s/ports", javaSourcePath(biz.BizID))
	return &backend.File{
		Path:    fmt.Sprintf("%s/%s", dir, PortFileName(domain)),
		Content: b.String(),
	}, nil
}

// GenerateBlessStarAdapter generates a Java adapter class for the given domain
func (j *JavaBackend) GenerateBlessStarAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	adapterName := AdapterTypeName(domain)
	pkg := JavaPackageFromBiz(biz.BizID)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("package %s.adapter.blessstar;\n\n", pkg))

	// Imports
	b.WriteString("import java.util.Map;\n")
	b.WriteString("import java.util.concurrent.ConcurrentHashMap;\n")
	b.WriteString(fmt.Sprintf("import %s.ports.ConfigReader;\n", pkg))
	b.WriteString(fmt.Sprintf("import %s.ports.%s;\n", pkg, interfaceName))
	b.WriteString("\n")

	// Class Javadoc
	b.WriteString("/**\n")
	b.WriteString(fmt.Sprintf(" * %s域配置的 BlessStar 适配器\n", domain))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(" * 内置三阶段降级: ConfigReader实时查询 -> LastKnownGood缓存 -> 硬编码默认值\n")
	b.WriteString(" * 请勿手动修改 — 由 blessstar-codegen 自动生成\n")
	b.WriteString(" */\n")
	b.WriteString(fmt.Sprintf("public class %s implements %s {\n\n", adapterName, interfaceName))

	// Fields
	b.WriteString("\tprivate final ConfigReader reader;\n")
	b.WriteString("\tprivate final Map<String, Object> lastKnownCache = new ConcurrentHashMap<>();\n")
	b.WriteString("\tprivate final Map<String, Object> hardcodedDefaults;\n\n")

	// Constructor
	b.WriteString("\t/**\n")
	b.WriteString(fmt.Sprintf("\t * 创建 %s 适配器实例\n", adapterName))
	b.WriteString("\t * @param reader 配置读取器，由业务方注入（可为 CachedReader、HTTPReader 等实现）\n")
	b.WriteString("\t */\n")
	b.WriteString(fmt.Sprintf("\tpublic %s(ConfigReader reader) {\n", adapterName))
	b.WriteString("\t\tthis.reader = reader;\n")
	b.WriteString("\t\tthis.hardcodedDefaults = new ConcurrentHashMap<>();\n")

	for _, c := range configs {
		javaType := JavaType(c.Type)
		methodName := MethodNameFromKey(c.Key)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = types.JavaBlessStarTypeZero(javaType)
		}
		b.WriteString(fmt.Sprintf("\t\tthis.hardcodedDefaults.put(%q, %s);\n", methodName, formatJavaDefault(javaType, defaultVal)))
	}

	b.WriteString("\t}\n\n")

	// Method implementations
	for _, c := range configs {
		javaType := JavaType(c.Type)
		methodName := MethodNameFromKey(c.Key)

		b.WriteString("\t@Override\n")
		b.WriteString(fmt.Sprintf("\tpublic %s %s() throws Exception {\n", javaType, methodName))

		// Step 1: Try ConfigReader
		registryPath := c.RegistryPath
		if registryPath == "" {
			registryPath = fmt.Sprintf("/config/%s/%s", biz.BizID, strings.ReplaceAll(c.Key, ".", "/"))
		}
		b.WriteString(fmt.Sprintf("\t\t// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）\n"))
		b.WriteString(fmt.Sprintf("\t\ttry {\n"))
		b.WriteString(fmt.Sprintf("\t\t\tObject val = reader.get(%q);\n", registryPath))
		b.WriteString(fmt.Sprintf("\t\t\tif (val != null) {\n"))
		b.WriteString(fmt.Sprintf("\t\t\t\tlastKnownCache.put(%q, val);\n", methodName))

		// Type-specific casting
		switch javaType {
		case "long":
			b.WriteString("\t\t\t\treturn ((Number) val).longValue();\n")
		case "int":
			b.WriteString("\t\t\t\treturn ((Number) val).intValue();\n")
		case "boolean":
			b.WriteString("\t\t\t\treturn (Boolean) val;\n")
		default:
			b.WriteString(fmt.Sprintf("\t\t\t\treturn (%s) val;\n", javaType))
		}
		b.WriteString("\t\t\t}\n")
		b.WriteString(fmt.Sprintf("\t\t} catch (Exception e) {\n"))
		b.WriteString(fmt.Sprintf("\t\t\t// 读取失败，继续降级\n"))
		b.WriteString(fmt.Sprintf("\t\t}\n\n"))

		// Step 2: Try last known good cache
		b.WriteString(fmt.Sprintf("\t\t// 第2阶段: 降级到 Last Known Good 缓存\n"))
		b.WriteString(fmt.Sprintf("\t\tObject cached = lastKnownCache.get(%q);\n", methodName))
		b.WriteString(fmt.Sprintf("\t\tif (cached != null) {\n"))
		switch javaType {
		case "long":
			b.WriteString("\t\t\treturn ((Number) cached).longValue();\n")
		case "int":
			b.WriteString("\t\t\treturn ((Number) cached).intValue();\n")
		case "boolean":
			b.WriteString("\t\t\treturn (Boolean) cached;\n")
		default:
			b.WriteString(fmt.Sprintf("\t\t\treturn (%s) cached;\n", javaType))
		}
		b.WriteString("\t\t}\n\n")

		// Step 3: Hardcoded default
		b.WriteString(fmt.Sprintf("\t\t// 第3阶段: 极冷启动 — 返回硬编码默认值\n"))
		b.WriteString(fmt.Sprintf("\t\tObject fallback = hardcodedDefaults.get(%q);\n", methodName))
		switch javaType {
		case "long":
			b.WriteString("\t\treturn ((Number) fallback).longValue();\n")
		case "int":
			b.WriteString("\t\treturn ((Number) fallback).intValue();\n")
		case "boolean":
			b.WriteString("\t\treturn (Boolean) fallback;\n")
		default:
			b.WriteString(fmt.Sprintf("\t\treturn (%s) fallback;\n", javaType))
		}
		b.WriteString("\t}\n\n")
	}

	b.WriteString("}\n")

	dir := fmt.Sprintf("%s/adapter/blessstar", javaSourcePath(biz.BizID))
	return &backend.File{
		Path:    fmt.Sprintf("%s/%s", dir, AdapterFileName(domain)),
		Content: b.String(),
	}, nil
}

// GenerateMockAdapter generates a Java mock class for the given domain
func (j *JavaBackend) GenerateMockAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	mockName := MockTypeName(domain)
	pkg := JavaPackageFromBiz(biz.BizID)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("package %s.adapter.mock;\n\n", pkg))

	b.WriteString(fmt.Sprintf("import %s.ports.%s;\n", pkg, interfaceName))
	b.WriteString("\n")

	// Class Javadoc
	b.WriteString("/**\n")
	b.WriteString(fmt.Sprintf(" * %s域配置的 Mock 实现（单元测试用）\n", domain))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(" * 请勿手动修改 — 由 blessstar-codegen 自动生成\n")
	b.WriteString(" */\n")
	b.WriteString(fmt.Sprintf("public class %s implements %s {\n\n", mockName, interfaceName))

	// Fields and constructor
	// Store values as fields with defaults from manifest
	for _, c := range configs {
		javaType := JavaType(c.Type)
		methodName := MethodNameFromKey(c.Key)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = types.JavaBlessStarTypeZero(javaType)
		}
		b.WriteString(fmt.Sprintf("\tprivate %s %s = %s;\n", javaType, lowerFirst(methodName), formatJavaDefault(javaType, defaultVal)))
	}
	b.WriteString("\n")

	// Constructor with defaults from manifest
	b.WriteString("\t/**\n")
	b.WriteString(fmt.Sprintf("\t * 创建默认 Mock（返回值按 manifest 默认值设定）\n"))
	b.WriteString("\t */\n")
	b.WriteString(fmt.Sprintf("\tpublic %s() {\n", mockName))
	b.WriteString("\t}\n\n")

	// Static factory method
	b.WriteString("\t/**\n")
	b.WriteString(fmt.Sprintf("\t * 创建默认 Mock 实例\n"))
	b.WriteString("\t */\n")
	b.WriteString(fmt.Sprintf("\tpublic static %s create() {\n", mockName))
	b.WriteString(fmt.Sprintf("\t\treturn new %s();\n", mockName))
	b.WriteString("\t}\n\n")

	// Fluent setter methods for each config
	for _, c := range configs {
		javaType := JavaType(c.Type)
		methodName := MethodNameFromKey(c.Key)
		label := LabelFromKey(c.Key, biz.ConfigLabels)

		b.WriteString("\t/**\n")
		b.WriteString(fmt.Sprintf("\t * 设置 %s\n", label))
		b.WriteString(fmt.Sprintf("\t * @param value %s 值\n", label))
		b.WriteString("\t * @return 当前 Mock 实例（支持链式调用）\n")
		b.WriteString("\t */\n")
		b.WriteString(fmt.Sprintf("\tpublic %s with%s(%s value) {\n", mockName, strings.TrimPrefix(methodName, "get"), javaType))
		b.WriteString(fmt.Sprintf("\t\tthis.%s = value;\n", lowerFirst(methodName)))
		b.WriteString("\t\treturn this;\n")
		b.WriteString("\t}\n\n")
	}

	// Method implementations
	for _, c := range configs {
		javaType := JavaType(c.Type)
		methodName := MethodNameFromKey(c.Key)

		b.WriteString("\t@Override\n")
		b.WriteString(fmt.Sprintf("\tpublic %s %s() throws Exception {\n", javaType, methodName))
		b.WriteString(fmt.Sprintf("\t\treturn this.%s;\n", lowerFirst(methodName)))
		b.WriteString("\t}\n\n")
	}

	b.WriteString("}\n")

	dir := fmt.Sprintf("%s/adapter/mock", javaSourcePath(biz.BizID))
	return &backend.File{
		Path:    fmt.Sprintf("%s/%s", dir, MockFileName(domain)),
		Content: b.String(),
	}, nil
}

// GenerateProvider generates a Java dependency injection provider
func (j *JavaBackend) GenerateProvider(biz *types.BizSystem) (*backend.File, error) {
	pkg := JavaPackageFromBiz(biz.BizID)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("package %s.provider;\n\n", pkg))

	// Imports
	b.WriteString(fmt.Sprintf("import %s.ports.ConfigReader;\n", pkg))
	sortedDomains := sortBizDomains(biz)
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		b.WriteString(fmt.Sprintf("import %s.ports.%s;\n", pkg, portName))
	}
	b.WriteString("\n")

	// Class Javadoc
	b.WriteString("/**\n")
	b.WriteString(fmt.Sprintf(" * BlessStar 配置依赖注入。\n"))
	b.WriteString(fmt.Sprintf(" * 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(" * 请勿手动修改 — 由 blessstar-codegen 自动生成\n")
	b.WriteString(" */\n")
	b.WriteString("public class BlessstarProvider {\n\n")

	// Adapters record-style class
	b.WriteString("\t/**\n")
	b.WriteString("\t * 持有所有域的配置适配器。\n")
	b.WriteString("\t */\n")
	b.WriteString("\tpublic static class Adapters {\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		fieldName := lowerFirst(portName)
		b.WriteString(fmt.Sprintf("\t\tprivate final %s %s;\n", portName, fieldName))
	}
	b.WriteString("\n")
	b.WriteString(fmt.Sprintf("\t\tpublic Adapters(\n"))
	first := true
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		fieldName := lowerFirst(portName)
		if first {
			b.WriteString(fmt.Sprintf("\t\t\t%s %s\n", portName, fieldName))
			first = false
		} else {
			b.WriteString(fmt.Sprintf("\t\t\t, %s %s\n", portName, fieldName))
		}
	}
	b.WriteString("\t\t) {\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		fieldName := lowerFirst(portName)
		b.WriteString(fmt.Sprintf("\t\t\tthis.%s = %s;\n", fieldName, fieldName))
	}
	b.WriteString("\t\t}\n\n")

	// Getters
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portName := PortInterfaceName(d)
		fieldName := lowerFirst(portName)
		b.WriteString(fmt.Sprintf("\t\tpublic %s get%s() {\n", portName, portName))
		b.WriteString(fmt.Sprintf("\t\t\treturn %s;\n", fieldName))
		b.WriteString("\t\t}\n")
	}

	b.WriteString("\t}\n\n")

	// Static provide method
	b.WriteString("\t/**\n")
	b.WriteString("\t * 一行实例化所有域配置适配器。\n")
	b.WriteString("\t * @param reader ConfigReader 实现（如 CachedReader、HTTPReader）\n")
	b.WriteString("\t * @return 包含所有域 Adapter 的 Adapters 实例\n")
	b.WriteString("\t */\n")
	b.WriteString("\tpublic static Adapters provideAdapters(ConfigReader reader) {\n")
	b.WriteString(fmt.Sprintf("\t\treturn new Adapters(\n"))
	first = true
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		adapterName := AdapterTypeName(d)
		if first {
			b.WriteString(fmt.Sprintf("\t\t\tnew %s.%s(reader)\n", pkg+".adapter.blessstar", adapterName))
			first = false
		} else {
			b.WriteString(fmt.Sprintf("\t\t\t, new %s.%s(reader)\n", pkg+".adapter.blessstar", adapterName))
		}
	}
	b.WriteString("\t\t);\n")
	b.WriteString("\t}\n")

	b.WriteString("}\n")

	return &backend.File{
		Path:    fmt.Sprintf("%s/provider/BlessstarProvider.java", javaSourcePath(biz.BizID)),
		Content: b.String(),
	}, nil
}

// GenerateGoMod generates a Maven pom.xml for Java
func (j *JavaBackend) GenerateGoMod(biz *types.BizSystem) (*backend.File, error) {
	groupId := JavaPackageFromBiz(biz.BizID)

	content := fmt.Sprintf(`<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>%s</groupId>
    <artifactId>%s-config</artifactId>
    <version>1.0.0</version>
    <packaging>jar</packaging>

    <name>%s Config Adapters</name>
    <description>自动生成的 BlessStar 配置适配器（%s）
请勿手动修改 — 由 blessstar-codegen 自动生成</description>

    <properties>
        <maven.compiler.source>17</maven.compiler.source>
        <maven.compiler.target>17</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
    </properties>

    <!-- 零外部依赖 — ConfigReader 由业务方自行实现 -->
    <!-- 无外部 SDK 依赖，生成的 adapter 代码仅依赖 JDK 标准库 -->
</project>
`, groupId, biz.BizID, biz.DisplayName, biz.BizID)

	return &backend.File{
		Path:    "pom.xml",
		Content: content,
	}, nil
}

// GenerateConfigReaderFile generates ConfigReader interface and CachedReader class
func (j *JavaBackend) GenerateConfigReaderFile(biz *types.BizSystem) ([]*backend.File, error) {
	pkg := JavaPackageFromBiz(biz.BizID)

	// Generate ports/ConfigReader.java
	configReaderContent := fmt.Sprintf(`package %s.ports;

/**
 * 配置读取接口，业务方自行选择实现方式。
 * 内置实现包括：CachedReader（秒级轮询缓存）、EnvReader（环境变量）、FileReader（本地文件）等。
 *
 * 实现类通过环境变量 BLESSSTAR_ENDPOINT 获取 Electron 地址（HTTPReader 场景）。
 * 初始化失败时不得阻塞进程，由 adapter 的三阶段降级兜底。
 *
 * get 返回 Object 以便 adapter 进行类型断言（与三阶段降级兼容）。
 * 常见返回类型：Long, Boolean, String, java.util.List&lt;String&gt;。
 * 业务方也可选择返回 JSON string 并在 adapter 外部自行解析。
 * 请勿手动修改 — 由 blessstar-codegen 自动生成
 */
public interface ConfigReader {
    /**
     * 读取一个配置值。
     * @param path 配置的完整注册路径（如 "/config/%s/auth/jwt/token_expiry_seconds"）
     * @return 配置值（可直接类型断言）或 null
     */
    Object get(String path);
}
`, pkg, biz.BizID)

	// Generate provider/CachedReader.java
	cachedReaderContent := fmt.Sprintf(`package %s.provider;

import java.util.Map;
import java.util.concurrent.*;
import java.util.function.Function;

import %s.ports.ConfigReader;

/**
 * 对 ConfigReader 的缓存包装器（可选装饰器）。
 * 后台线程使用 ScheduledExecutorService 定时刷新缓存，实现秒级准实时热更新。
 * 不依赖任何外部库，仅使用 JDK 标准库。
 *
 * 使用方式（由 main 注入）：
 * <pre>
 *     ConfigReader rawReader = new HttpReader();  // 从环境变量 BLESSSTAR_ENDPOINT 读取地址
 *     CachedReader cachedReader = new CachedReader(rawReader, 30, TimeUnit.SECONDS);
 *     Adapters adapters = BlessstarProvider.provideAdapters(cachedReader);
 * </pre>
 */
public class CachedReader implements ConfigReader {
    private final ConfigReader inner;
    private final Map<String, Object> cache = new ConcurrentHashMap<>();
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private Runnable refreshFunc = () -> {};

    /**
     * 创建 CachedReader 实例。
     * @param inner     底层 ConfigReader 实现
     * @param interval  缓存刷新周期
     * @param unit      时间单位
     */
    public CachedReader(ConfigReader inner, long interval, TimeUnit unit) {
        this.inner = inner;
        scheduler.scheduleAtFixedRate(this::refreshLoop, interval, interval, unit);
    }

    /**
     * 设置全量刷新回调函数，由业务方自定义需要缓存的配置路径。
     * @param fn 刷新回调
     * @return 当前 CachedReader 实例
     */
    public CachedReader withRefreshFunc(Runnable fn) {
        this.refreshFunc = fn;
        return this;
    }

    @Override
    public Object get(String path) {
        Object val = cache.get(path);
        if (val != null) {
            return val;
        }
        return inner.get(path);
    }

    private void refreshLoop() {
        try {
            refreshFunc.run();
        } catch (Exception e) {
            // 刷新失败不影响现有缓存，仅跳过本轮
        }
    }

    /**
     * 停止后台刷新线程。
     */
    public void shutdown() {
        scheduler.shutdown();
    }
}
`, pkg, pkg)

	dir := javaSourcePath(biz.BizID)
	return []*backend.File{
		{
			Path:    fmt.Sprintf("%s/ports/ConfigReader.java", dir),
			Content: configReaderContent,
		},
		{
			Path:    fmt.Sprintf("%s/provider/CachedReader.java", dir),
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

// lowerFirst returns the string with the first character lowercased
func lowerFirst(s string) string {
	if len(s) == 0 {
		return s
	}
	return string(s[0]+32) + s[1:]
}

// Ensure interface compliance
var _ backend.LanguageBackend = (*JavaBackend)(nil)
