package types

// ConfigField represents a single configuration field from manifest.json or config_metadata.json
type ConfigField struct {
	Key             string            `json:"key"`
	Type            string            `json:"type"`
	Default         string            `json:"default"`
	Description     string            `json:"description"`
	Required        bool              `json:"required"`
	RegistryPath    string            `json:"registry_path,omitempty"`
	BizID           string            `json:"biz_id,omitempty"`
	BusinessDomain  string            `json:"business_domain,omitempty"`
	ValueRange      string            `json:"value_range_suggestion,omitempty"`
	UIOrder         int               `json:"ui_order,omitempty"` // from ui_metadata
	EnumValues      []string          `json:"enum_values,omitempty"`
	Pattern         string            `json:"pattern,omitempty"`
	SearchKeywords  []string          `json:"search_keywords,omitempty"`
	AIHint          string            `json:"ai_hint,omitempty"`
	ImpactScope     []string          `json:"impact_scope,omitempty"`
}

// DomainShard represents a business domain shard
type DomainShard struct {
	DomainName        string   `json:"domainName"`
	Keywords          []string `json:"keywords"`
	DomainDescription string   `json:"domainDescription"`
	ConfigKeys        []string `json:"configKeys"`
}

// Manifest represents the top-level manifest.json structure
type Manifest struct {
	BizID       string        `json:"biz_id"`
	DisplayName string        `json:"display_name"`
	Description string        `json:"description"`
	Version     string        `json:"version"`
	SDKVersion  string        `json:"sdk_version"`
	Fields      []ConfigField `json:"fields"`
	AIData      ManifestAIData `json:"ai_data"`
}

// ManifestAIData represents the ai_data section of manifest.json
type ManifestAIData struct {
	Summary            string                   `json:"summary"`
	BusinessCapabilities []string               `json:"business_capabilities"`
	ConfigDomains      map[string]string        `json:"config_domains"`
	ConfigLabels       map[string]string        `json:"configLabels"`
	InvertedIndex      []InvertedIndexEntry     `json:"invertedIndex"`
	SkillRoutes        []SkillRoute             `json:"skillRoutes"`
}

// InvertedIndexEntry maps a keyword to its related config keys
type InvertedIndexEntry struct {
	Keyword   string   `json:"keyword"`
	ConfigKeys []string `json:"configKeys"`
}

// SkillRoute represents an AI skill routing entry
type SkillRoute struct {
	Prefix      string   `json:"prefix"`
	Description string   `json:"description"`
	ToolChain   []string `json:"toolChain"`
	Priority    int      `json:"priority"`
}

// CodegenConfig holds the code generation configuration
type CodegenConfig struct {
	Language    string
	Manifest    string
	Metadata    string
	OutputDir   string
}

// BizSystem is the aggregated business system model ready for code generation
type BizSystem struct {
	BizID                string
	DisplayName          string
	Description          string
	ConfigsByDomain      map[string][]ConfigField
	DomainShards         []DomainShard
	ConfigLabels         map[string]string
	AllConfigs           []ConfigField
}

// GoBlessStarType maps BlessStar config types to Go types
func GoBlessStarType(bsType string) string {
	switch bsType {
	case "I64":
		return "int64"
	case "I32":
		return "int32"
	case "STR":
		return "string"
	case "BOOL":
		return "bool"
	case "ARR":
		return "[]string"
	case "ENUM":
		return "string"
	default:
		return "string"
	}
}

// CBlessStarType maps BlessStar config types to C types
func CBlessStarType(bsType string) string {
	switch bsType {
	case "I64":
		return "int64_t"
	case "I32":
		return "int32_t"
	case "STR":
		return "const char*"
	case "BOOL":
		return "bool"
	case "ARR":
		return "const char**"
	case "ENUM":
		return "const char*"
	default:
		return "const char*"
	}
}

// CBlessStarTypeZero returns the zero value for the C type (takes C type string)
func CBlessStarTypeZero(cType string) string {
	switch cType {
	case "int64_t":
		return "0"
	case "int32_t":
		return "0"
	case "const char*":
		return "NULL"
	case "bool":
		return "false"
	case "const char**":
		return "NULL"
	default:
		return "NULL"
	}
}

// GoBlessStarTypeZero returns the zero value for the Go type
func GoBlessStarTypeZero(bsType string) string {
	switch bsType {
	case "int64":
		return "0"
	case "int32":
		return "0"
	case "string":
		return `""`
	case "bool":
		return "false"
	case "[]string":
		return "nil"
	default:
		return `""`
	}
}

// JavaBlessStarType maps BlessStar config types to Java types
func JavaBlessStarType(bsType string) string {
	switch bsType {
	case "I64":
		return "long"
	case "I32":
		return "int"
	case "STR":
		return "String"
	case "BOOL":
		return "boolean"
	case "ARR":
		return "java.util.List<String>"
	case "ENUM":
		return "String"
	default:
		return "String"
	}
}

// JavaBlessStarTypeZero returns the zero/default value literal for the Java type
func JavaBlessStarTypeZero(bsType string) string {
	switch bsType {
	case "long":
		return "0L"
	case "int":
		return "0"
	case "String":
		return `""`
	case "boolean":
		return "false"
	case "java.util.List<String>":
		return "null"
	default:
		return `""`
	}
}

// PythonBlessStarType maps BlessStar config types to Python types
func PythonBlessStarType(bsType string) string {
	switch bsType {
	case "I64":
		return "int"
	case "I32":
		return "int"
	case "STR":
		return "str"
	case "BOOL":
		return "bool"
	case "ARR":
		return "list[str]"
	case "ENUM":
		return "str"
	default:
		return "str"
	}
}

// PythonBlessStarTypeZero returns the zero/default value for the Python type (as a python literal string)
func PythonBlessStarTypeZero(pyType string) string {
	switch pyType {
	case "int":
		return "0"
	case "str":
		return `""`
	case "bool":
		return "False"
	case "list[str]":
		return "[]"
	default:
		return `""`
	}
}
