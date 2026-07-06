package python_backend

import (
	"fmt"
	"sort"
	"strings"

	"github.com/blessstar/blessstar-codegen/backend"
	"github.com/blessstar/blessstar-codegen/types"
)

// PythonBackend implements the LanguageBackend for Python
type PythonBackend struct{}

func New() *PythonBackend { return &PythonBackend{} }

func (p *PythonBackend) Name() string          { return "python" }
func (p *PythonBackend) FileExtension() string { return ".py" }
func (p *PythonBackend) CommentPrefix() string { return "#" }

// ConfigDomainPortName maps a Chinese business domain name to a Python port interface name
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

// PortInterfaceName returns the Python Protocol class name
func PortInterfaceName(domain string) string {
	return ConfigDomainPortName(domain) + "Config"
}

// PortFileName returns the file name for the port
func PortFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + ".py"
}

// AdapterTypeName returns the Python class name for the BlessStar adapter
func AdapterTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigAdapter"
}

// MockTypeName returns the Python class name for the mock adapter
func MockTypeName(domain string) string {
	return ConfigDomainPortName(domain) + "ConfigMock"
}

// AdapterFileName returns the file name for the adapter
func AdapterFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "_adapter.py"
}

// MockFileName returns the file name for the mock
func MockFileName(domain string) string {
	return strings.ToLower(ConfigDomainPortName(domain)) + "_mock.py"
}

// PackageNameFromBiz returns a valid Python package name from biz_id.
func PackageNameFromBiz(bizID string) string {
	return strings.ReplaceAll(bizID, "-", "_")
}

// PythonType maps a BlessStar type to Python
func PythonType(bsType string) string {
	return types.PythonBlessStarType(bsType)
}

// PortModuleName returns the Python module name for a domain (file name without .py extension)
func PortModuleName(domain string) string {
	return strings.TrimSuffix(PortFileName(domain), ".py")
}

// MethodNameFromKey maps a config key to a Python method name (snake_case).
// Examples:
//
//	"auth.jwt.token_expiry_seconds" → "jwt_token_expiry_seconds"
//	"user.role.values" → "role_values"
func MethodNameFromKey(key string) string {
	parts := strings.Split(key, ".")
	// Remove domain prefix (e.g., "auth") to get the method name
	if len(parts) >= 2 {
		parts = parts[1:]
	}
	return strings.Join(parts, "_")
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

// generateFieldDoc generates Python docstring for a config field
func generateFieldDoc(field types.ConfigField, labels map[string]string) string {
	label := LabelFromKey(field.Key, labels)
	desc := field.Description
	if desc == "" {
		desc = field.AIHint
	}
	rangeHint := field.ValueRange
	methodName := MethodNameFromKey(field.Key)
	pyType := PythonType(field.Type)

	var lines []string
	lines = append(lines, fmt.Sprintf("    def %s(self, ctx: AbstractContextManager) -> %s:", methodName, pyType))
	lines = append(lines, fmt.Sprintf("        \"\"\"%s (%s)", label, field.Key))
	if desc != "" {
		lines = append(lines, fmt.Sprintf("        描述: %s", desc))
	}
	if rangeHint != "" {
		lines = append(lines, fmt.Sprintf("        建议值: %s", rangeHint))
	}
	lines = append(lines, fmt.Sprintf("        类型: %s", field.Type))
	lines = append(lines, "        \"\"\"")
	lines = append(lines, "        ...")
	return strings.Join(lines, "\n")
}

func (p *PythonBackend) GeneratePortInterface(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("# Package ports — 自动生成于 BlessStar 配置端口-适配器\n"))
	b.WriteString(fmt.Sprintf("# 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("# 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf("# 请勿手动修改 — 由 blessstar-codegen 自动生成\n"))
	b.WriteString(fmt.Sprintf("# Source: manifest.json\n\n"))
	b.WriteString("from typing import Protocol\n")
	b.WriteString("from contextlib import AbstractContextManager\n\n\n")
	b.WriteString(fmt.Sprintf("class %s(Protocol):\n", interfaceName))
	b.WriteString(fmt.Sprintf("    \"\"\"%s域配置接口\"\"\"\n", domain))
	b.WriteString("\n")
	for _, c := range configs {
		b.WriteString(generateFieldDoc(c, biz.ConfigLabels))
		b.WriteString("\n")
	}
	// Ensure trailing newline
	b.WriteString("\n")

	return &backend.File{
		Path:    fmt.Sprintf("ports/%s", PortFileName(domain)),
		Content: b.String(),
	}, nil
}

func (p *PythonBackend) GenerateBlessStarAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	adapterName := AdapterTypeName(domain)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("# Package adapter_blessstar — 自动生成于 BlessStar 配置端口-适配器\n"))
	b.WriteString(fmt.Sprintf("# 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("# 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf("# 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n"))
	b.WriteString("from typing import Any, Dict\n")
	b.WriteString("from contextlib import AbstractContextManager\n")
	b.WriteString("from ports.config_reader import ConfigReader\n")
	b.WriteString(fmt.Sprintf("from ports.%s import %s\n\n\n", PortModuleName(domain), interfaceName))
	b.WriteString(fmt.Sprintf("class %s(%s):\n", adapterName, interfaceName))
	b.WriteString(fmt.Sprintf("    \"\"\"%s域配置的 BlessStar 适配器\n", domain))
	b.WriteString("    内置三阶段降级: ConfigReader实时查询 → 缓存 → 硬编码默认值\n")
	b.WriteString(fmt.Sprintf("    \"\"\"\n\n"))
	b.WriteString("    def __init__(self, reader: ConfigReader) -> None:\n")
	b.WriteString("        self._reader = reader\n")
	b.WriteString("        self._cache: Dict[str, Any] = {}\n")
	b.WriteString("        self._defaults: Dict[str, Any] = {\n")
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		pyType := PythonType(c.Type)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = types.PythonBlessStarTypeZero(pyType)
		}
		b.WriteString(fmt.Sprintf("            %q: %s,\n", methodName, formatPyDefault(pyType, defaultVal)))
	}
	b.WriteString("        }\n\n")

	// Method implementations
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		pyType := PythonType(c.Type)
		registryPath := c.RegistryPath
		if registryPath == "" {
			registryPath = fmt.Sprintf("/config/%s/%s", biz.BizID, strings.ReplaceAll(c.Key, ".", "/"))
		}

		b.WriteString(fmt.Sprintf("    def %s(self, ctx: AbstractContextManager) -> %s:\n", methodName, pyType))
		b.WriteString(fmt.Sprintf("        path = %q\n", registryPath))
		b.WriteString("        # Stage 1: ConfigReader 实时查询\n")
		b.WriteString("        try:\n")
		b.WriteString(fmt.Sprintf("            val = self._reader.get(ctx, path)\n"))
		b.WriteString(fmt.Sprintf("            self._cache[%q] = val\n", methodName))
		b.WriteString(fmt.Sprintf("            return %s(val)\n", pyType))
		b.WriteString("        except Exception:\n")
		b.WriteString("            pass\n")
		b.WriteString("        # Stage 2: 降级到 Last Known Good 缓存\n")
		b.WriteString(fmt.Sprintf("        if %q in self._cache:\n", methodName))
		b.WriteString(fmt.Sprintf("            return %s(self._cache[%q])\n", pyType, methodName))
		b.WriteString("        # Stage 3: 极冷启动 — 返回硬编码默认值\n")
		b.WriteString(fmt.Sprintf("        return %s(self._defaults[%q])\n", pyType, methodName))
		b.WriteString("\n")
	}

	return &backend.File{
		Path:    fmt.Sprintf("adapters/blessstar/%s", AdapterFileName(domain)),
		Content: b.String(),
	}, nil
}

func (p *PythonBackend) GenerateMockAdapter(biz *types.BizSystem, domain string, configs []types.ConfigField) (*backend.File, error) {
	if len(configs) == 0 {
		return nil, nil
	}

	interfaceName := PortInterfaceName(domain)
	mockName := MockTypeName(domain)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("# Package adapter_mock — 自动生成于 BlessStar 配置 Mock\n"))
	b.WriteString(fmt.Sprintf("# 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("# 领域: %s\n", domain))
	b.WriteString(fmt.Sprintf("# 专为单元测试设计 — 固定返回值\n\n"))
	b.WriteString("from typing import Callable\n")
	b.WriteString("from contextlib import AbstractContextManager\n")
	b.WriteString(fmt.Sprintf("from ports.%s import %s\n\n\n", PortModuleName(domain), interfaceName))
	b.WriteString(fmt.Sprintf("class %s(%s):\n", mockName, interfaceName))
	b.WriteString(fmt.Sprintf("    \"\"\"%s域配置的 Mock 实现（单元测试用）\"\"\"\n\n", domain))
	b.WriteString("    def __init__(self) -> None:\n")
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		pyType := PythonType(c.Type)
		defaultVal := c.Default
		if defaultVal == "" {
			defaultVal = types.PythonBlessStarTypeZero(pyType)
		}
		b.WriteString(fmt.Sprintf("        self.%s_func: Callable[[AbstractContextManager], %s] = lambda ctx: %s\n",
			methodName, pyType, formatPyDefault(pyType, defaultVal)))
	}
	b.WriteString("\n")
	b.WriteString("    @classmethod\n")
	b.WriteString(fmt.Sprintf("    def create_default(cls) -> %s:\n", interfaceName))
	b.WriteString(fmt.Sprintf("        \"\"\"创建默认 Mock 实例（返回值按 manifest 默认值设定）\"\"\"\n"))
	b.WriteString(fmt.Sprintf("        return cls()\n"))
	b.WriteString("\n")

	// Method implementations
	for _, c := range configs {
		methodName := MethodNameFromKey(c.Key)
		pyType := PythonType(c.Type)
		b.WriteString(fmt.Sprintf("    def %s(self, ctx: AbstractContextManager) -> %s:\n", methodName, pyType))
		b.WriteString(fmt.Sprintf("        return self.%s_func(ctx)\n", methodName))
		b.WriteString("\n")
	}

	return &backend.File{
		Path:    fmt.Sprintf("adapters/mock/%s", MockFileName(domain)),
		Content: b.String(),
	}, nil
}

func (p *PythonBackend) GenerateProvider(biz *types.BizSystem) (*backend.File, error) {
	packageName := PackageNameFromBiz(biz.BizID)

	var b strings.Builder
	b.WriteString(fmt.Sprintf("# Package %s — 自动生成于 BlessStar 配置依赖注入\n", packageName))
	b.WriteString(fmt.Sprintf("# 业务系统: %s (%s)\n", biz.DisplayName, biz.BizID))
	b.WriteString(fmt.Sprintf("# 请勿手动修改 — 由 blessstar-codegen 自动生成\n\n"))
	b.WriteString("from dataclasses import dataclass\n")
	b.WriteString("from ports.config_reader import ConfigReader\n")
	b.WriteString("from adapters import blessstar as adapter_blessstar\n")
	sortedDomains := sortBizDomains(biz)
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		portFile := PortModuleName(d)
		interfaceName := PortInterfaceName(d)
		b.WriteString(fmt.Sprintf("from ports.%s import %s\n", portFile, interfaceName))
	}
	b.WriteString("\n\n")

	// Adapters dataclass
	b.WriteString("@dataclass\n")
	b.WriteString("class Adapters:\n")
	b.WriteString("    \"\"\"持有所有域的配置适配器\"\"\"\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		interfaceName := PortInterfaceName(d)
		fieldName := toSnakeCase(interfaceName)
		b.WriteString(fmt.Sprintf("    %s: %s\n", fieldName, interfaceName))
	}
	b.WriteString("\n")

	// Provide function
	b.WriteString("def provide_blessstar_adapters(reader: ConfigReader) -> Adapters:\n")
	b.WriteString("    \"\"\"一行实例化所有域配置适配器\n\n")
	b.WriteString("    注入 ConfigReader 后返回所有域的 Port 实现\n")
	b.WriteString("    reader 由业务方提供（可为 CachedReader、HTTPReader、EnvReader 等实现）\n")
	b.WriteString("    \"\"\"\n")
	b.WriteString("    return Adapters(\n")
	for _, d := range sortedDomains {
		configs := biz.ConfigsByDomain[d]
		if len(configs) == 0 {
			continue
		}
		interfaceName := PortInterfaceName(d)
		adapterName := AdapterTypeName(d)
		fieldName := toSnakeCase(interfaceName)
		b.WriteString(fmt.Sprintf("        %s=adapter_blessstar.%s(reader),\n", fieldName, adapterName))
	}
	b.WriteString("    )\n")

	return &backend.File{
		Path:    "provider/blessstar_provider.py",
		Content: b.String(),
	}, nil
}

func (p *PythonBackend) GenerateGoMod(biz *types.BizSystem) (*backend.File, error) {
	// Python uses pyproject.toml; returning nil as Python does not require this step.
	return nil, nil
}

func (p *PythonBackend) GenerateConfigReaderFile(biz *types.BizSystem) ([]*backend.File, error) {
	// Generate ports/config_reader.py — the ConfigReader Protocol
	configReaderContent := fmt.Sprintf(`# Package ports — 提供业务系统的配置读取接口。
# 所有 Port 接口定义在此包中，业务代码仅依赖此包。
# 请勿手动修改 — 由 blessstar-codegen 自动生成

from typing import Protocol
from contextlib import AbstractContextManager


class ConfigReader(Protocol):
    """配置读取接口，业务方自行选择实现方式。

    内置实现包括：CachedReader（秒级轮询缓存）、EnvReader（环境变量）、FileReader（本地文件）等。

    实现类通过环境变量 BLESSSTAR_ENDPOINT 获取 Electron 地址（HTTPReader 场景）。
    初始化失败时不得阻塞进程，由 adapter 的三阶段降级兜底。

    get 返回任意类型以便 adapter 进行类型转换（与三阶段降级兼容）。
    常见返回类型：int, bool, str, list[str]。
    """

    def get(self, ctx: AbstractContextManager, path: str):
        """读取一个配置值。

        Args:
            ctx: 上下文管理器（业务上下文）。
            path: 配置的完整注册路径（如 "/config/%[1]s/auth/jwt/token_expiry_seconds"）。

        Returns:
            配置值（可直接用于类型转换）或抛出异常。
        """
        ...
`, biz.BizID)

	// Generate provider/cached_reader.py — optional CachedReader decorator
	cachedReaderContent := fmt.Sprintf(`# Package %[1]s — 提供 BlessStar 配置的依赖注入和可选装饰器。
# 请勿手动修改 — 由 blessstar-codegen 自动生成

import threading
import time
from typing import Any, Dict
from contextlib import AbstractContextManager
from ports.config_reader import ConfigReader


class CachedReader:
    """对 ConfigReader 的缓存包装器（可选装饰器）。

    后台线程定时刷新缓存，实现秒级准实时热更新。
    不依赖任何外部库，仅使用 Python 标准库。

    使用方式（由 main 入口注入）:

        raw_reader = HttpReader()                           # 从环境变量 BLESSSTAR_ENDPOINT 读取地址
        cached_reader = CachedReader(raw_reader, interval=30)
        adapters = provide_blessstar_adapters(cached_reader)
    """

    def __init__(self, inner: ConfigReader, interval: float = 30.0) -> None:
        """创建 CachedReader 实例。

        Args:
            inner: 底层 ConfigReader 实现。
            interval: 缓存刷新周期（秒），默认 30 秒。
        """
        self._inner = inner
        self._cache: Dict[str, Any] = {}
        self._interval = interval
        self._refresh_func = lambda ctx: None  # type: ignore
        self._stop_event = threading.Event()
        self._thread = threading.Thread(target=self._refresh_loop, daemon=True)
        self._thread.start()

    def set_refresh_func(self, fn) -> "CachedReader":
        """设置全量刷新回调函数，由业务方自定义需要缓存的配置路径。"""
        self._refresh_func = fn
        return self

    def get(self, ctx: AbstractContextManager, path: str):
        """从缓存中读取配置值。若缓存命中直接返回，否则穿透到底层 ConfigReader。"""
        if path in self._cache:
            return self._cache[path]
        return self._inner.get(ctx, path)

    def _refresh_loop(self) -> None:
        """后台线程，定时执行全量刷新。"""
        while not self._stop_event.is_set():
            time.sleep(self._interval)
            try:
                self._refresh_func(None)
            except Exception:
                # 刷新失败不影响现有缓存，仅跳过本轮
                pass

    def close(self) -> None:
        """停止后台刷新线程。"""
        self._stop_event.set()
        self._thread.join(timeout=5)
`, biz.BizID)

	return []*backend.File{
		{
			Path:    "ports/config_reader.py",
			Content: configReaderContent,
		},
		{
			Path:    "provider/cached_reader.py",
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

// toSnakeCase converts CamelCase to snake_case.
// Example: "AuthConfig" → "auth_config"
func toSnakeCase(s string) string {
	var result strings.Builder
	for i, r := range s {
		if r >= 'A' && r <= 'Z' {
			if i > 0 {
				result.WriteRune('_')
			}
			result.WriteRune(r - 'A' + 'a')
		} else {
			result.WriteRune(r)
		}
	}
	return result.String()
}

// formatPyDefault formats a default value as a Python literal
func formatPyDefault(pyType, defaultVal string) string {
	switch {
	case pyType == "int":
		if defaultVal == "" {
			return "0"
		}
		return defaultVal
	case pyType == "bool":
		if defaultVal == "true" || defaultVal == "1" || defaultVal == "True" {
			return "True"
		}
		return "False"
	case pyType == "str":
		if defaultVal == "" {
			return `""`
		}
		return fmt.Sprintf("%q", defaultVal)
	case pyType == "list[str]":
		if defaultVal == "" || defaultVal == "null" {
			return "[]"
		}
		// Parse JSON array like ["*", "example.com"] → ["*", "example.com"]
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
			return `""`
		}
		return fmt.Sprintf("%q", defaultVal)
	}
}

// Ensure all interfaces are satisfied
var _ backend.LanguageBackend = (*PythonBackend)(nil)
