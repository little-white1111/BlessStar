package parser

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"

	"github.com/blessstar/blessstar-codegen/types"
)

// ─── 简易 YAML 解析器（零外部依赖） ───

// yamlNode represents a parsed YAML node
type yamlNode struct {
	key   string
	value string       // scalar value
	kind  yamlNodeKind // "scalar", "mapping", "sequence"
	nodes []yamlNode   // children (for mapping/sequence)
}

type yamlNodeKind int

const (
	yamlScalar   yamlNodeKind = iota // key: value
	yamlMapping                      // key:\n  indented children
	yamlSequence                     // - item  or  - key: value
)

// ParseSchemaYAML 解析 config-schema.yaml 文件
func ParseSchemaYAML(path string) (*types.ConfigSchema, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("failed to open schema file %s: %w", path, err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	var lines []string
	for scanner.Scan() {
		lines = append(lines, scanner.Text())
	}
	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("failed to read schema file %s: %w", path, err)
	}

	root := parseYAMLNodes(lines, 0)
	schema := &types.ConfigSchema{}

	for _, node := range root {
		switch node.key {
		case "domain":
			if node.kind == yamlScalar {
				schema.Domain = node.value
			}
		case "version":
			if node.kind == yamlScalar {
				schema.Version = node.value
			}
		case "fields":
			if node.kind == yamlSequence {
				for _, item := range node.nodes {
					field := parseFieldNode(item)
					schema.Fields = append(schema.Fields, field)
				}
			}
		}
	}

	return schema, nil
}

// parseFieldNode parses a single field entry from YAML sequence
func parseFieldNode(node yamlNode) types.ConfigSchemaField {
	field := types.ConfigSchemaField{}

	// Handle case where the first mapping entry (e.g. "key: value") is at the
	// parent node level rather than in children. YAML sequence items like:
	//   - key: auth.jwt.token_expiry_seconds
	//     type: I64
	// produce node.key="key", node.value="auth.jwt.token_expiry_seconds",
	// with type/I64 in node.nodes.
	if node.key != "" && node.value != "" {
		switch node.key {
		case "key":
			field.Key = node.value
		case "type":
			field.Type = node.value
		case "default":
			field.Default = node.value
		case "business_desc":
			field.BusinessDesc = node.value
		case "ai_hint":
			field.AIHint = node.value
		}
	}

	for _, n := range node.nodes {
		switch n.key {
		case "key":
			field.Key = n.value
		case "type":
			field.Type = n.value
		case "default":
			field.Default = n.value
		case "business_desc":
			field.BusinessDesc = n.value
		case "impact_scope":
			field.ImpactScope = parseStringList(n)
		case "search_keywords":
			field.SearchKeywords = parseStringList(n)
		case "ai_hint":
			field.AIHint = n.value
		case "enum_values":
			field.EnumValues = parseStringList(n)
		case "contract":
			field.Contract = parseContractNode(n)
		case "env_overrides":
			field.EnvOverrides = parseEnvOverrides(n)
		case "ui_meta":
			field.UIMeta = parseUIMeta(n)
		}
	}
	return field
}

// parseContractNode parses a contract mapping node
func parseContractNode(node yamlNode) *types.ContractDef {
	if node.kind != yamlMapping {
		return nil
	}
	cd := &types.ContractDef{}
	for _, n := range node.nodes {
		switch n.key {
		case "range":
			cd.Range = parseInt64List(n)
		case "dependencies":
			cd.Dependencies = parseStringList(n)
		case "slo_impact":
			cd.SLOImpact = n.value
		case "approval_required":
			cd.ApprovalRequired = n.value == "true"
		}
	}
	return cd
}

// parseEnvOverrides parses env_overrides mapping (env_name -> {range: [...]})
func parseEnvOverrides(node yamlNode) map[string]types.EnvOverride {
	if node.kind != yamlMapping {
		return nil
	}
	overrides := make(map[string]types.EnvOverride)
	for _, n := range node.nodes {
		eo := types.EnvOverride{}
		for _, inner := range n.nodes {
			if inner.key == "range" {
				eo.Range = parseInt64List(inner)
			}
		}
		overrides[n.key] = eo
	}
	return overrides
}

// parseUIMeta parses ui_meta mapping node
func parseUIMeta(node yamlNode) *types.UIMetaDef {
	if node.kind != yamlMapping {
		return nil
	}
	ui := &types.UIMetaDef{}
	for _, n := range node.nodes {
		switch n.key {
		case "label":
			ui.Label = n.value
		case "order":
			if v, err := strconv.Atoi(n.value); err == nil {
				ui.Order = v
			}
		}
	}
	return ui
}

// parseStringList parses a YAML sequence node into []string
func parseStringList(node yamlNode) []string {
	if node.kind != yamlSequence {
		// Some values might be inline JSON arrays like '["a", "b"]'
		if node.kind == yamlScalar && strings.HasPrefix(node.value, "[") {
			return parseInlineJSONArray(node.value)
		}
		return nil
	}
	var result []string
	for _, n := range node.nodes {
		result = append(result, unquoteYAMLValue(n.value))
	}
	return result
}

// parseInt64List parses a YAML sequence node or inline JSON array into []int64
func parseInt64List(node yamlNode) []int64 {
	if node.kind == yamlSequence && len(node.nodes) > 0 {
		var result []int64
		for _, n := range node.nodes {
			if v, err := strconv.ParseInt(strings.TrimSpace(n.value), 10, 64); err == nil {
				result = append(result, v)
			}
		}
		return result
	}
	// Handle inline JSON array format like [60, 43200]
	if node.kind == yamlScalar && strings.HasPrefix(node.value, "[") {
		strs := parseInlineJSONArray(node.value)
		var result []int64
		for _, s := range strs {
			if v, err := strconv.ParseInt(strings.TrimSpace(s), 10, 64); err == nil {
				result = append(result, v)
			}
		}
		return result
	}
	return nil
}

// parseInlineJSONArray parses a JSON-like array string into []string
func parseInlineJSONArray(s string) []string {
	s = strings.TrimSpace(s)
	s = strings.TrimPrefix(s, "[")
	s = strings.TrimSuffix(s, "]")
	if s == "" {
		return nil
	}
	parts := strings.Split(s, ",")
	var result []string
	for _, p := range parts {
		p = strings.TrimSpace(p)
		p = strings.Trim(p, "\"")
		result = append(result, p)
	}
	return result
}

// unquoteYAMLValue removes surrounding quotes from a YAML scalar value
func unquoteYAMLValue(s string) string {
	s = strings.TrimSpace(s)
	if len(s) >= 2 {
		if (s[0] == '"' && s[len(s)-1] == '"') || (s[0] == '\'' && s[len(s)-1] == '\'') {
			return s[1 : len(s)-1]
		}
	}
	return s
}

// ─── YAML 行解析器 ───

// parseYAMLNodes parses lines into a tree of yamlNodes starting at the given indentation level.
// Returns the list of top-level nodes at that indentation.
func parseYAMLNodes(lines []string, baseIndent int) []yamlNode {
	var nodes []yamlNode
	var childLines []string
	childBase := 0
	inChild := false

	for i := 0; i < len(lines); i++ {
		line := lines[i]
		trimmed := strings.TrimSpace(line)

		// Skip empty lines and comments
		if trimmed == "" || strings.HasPrefix(trimmed, "#") {
			continue
		}

		indent := countIndent(line)

		// If we're collecting children, check if this line is at the child indentation level
		if inChild {
			if indent <= baseIndent {
				// Back to parent level — process collected children
				children := parseYAMLNodes(childLines, childBase)
				if len(nodes) > 0 {
					nodes[len(nodes)-1].nodes = children
				}
				childLines = nil
				inChild = false
				// Fall through to process this line as a new node
			} else if indent == childBase {
				// Another child at the same level — append child lines and continue
				childLines = append(childLines, line)
				continue
			} else if indent > childBase {
				// Still a child of the current child — continue collecting
				childLines = append(childLines, line)
				continue
			}
		}

		if !inChild && len(nodes) > 0 && nodes[len(nodes)-1].kind != yamlScalar && len(childLines) > 0 {
			children := parseYAMLNodes(childLines, childBase)
			nodes[len(nodes)-1].nodes = children
			childLines = nil
		}

		// Helper: look ahead to collect child lines at deeper indentation.
		// Returns (childLines, childBase, lastConsumedIndex).
		collectChildren := func(startIdx, lineIndent int) ([]string, int, int) {
			var cl []string
			cb := lineIndent
			last := startIdx - 1
			for j := startIdx; j < len(lines); j++ {
				nl := lines[j]
				nt := strings.TrimSpace(nl)
				if nt == "" || strings.HasPrefix(nt, "#") {
					continue
				}
				ni := countIndent(nl)
				if ni > lineIndent {
					if cb == lineIndent {
						cb = ni
					}
					cl = append(cl, nl)
					last = j
				} else {
					break
				}
			}
			return cl, cb, last
		}

		// Parse this line
		if strings.HasPrefix(trimmed, "- ") || trimmed == "-" {
			// Sequence item
			item := yamlNode{kind: yamlScalar}
			rest := strings.TrimPrefix(trimmed, "- ")
			rest = strings.TrimSpace(rest)

			if rest == "" {
				// The sequence item is a mapping, look ahead for children
				item.kind = yamlMapping
				item.key = ""
				cl, cb, lastIdx := collectChildren(i+1, countIndent(line))
				childLines = cl
				childBase = cb
				if lastIdx >= i {
					i = lastIdx
				}
				inChild = true
				nodes = append(nodes, item)
				continue
			}

			// Check if the rest is "key: value" or "key:"
			if colonIdx := strings.Index(rest, ":"); colonIdx >= 0 {
				item.kind = yamlMapping
				item.key = strings.TrimSpace(rest[:colonIdx])
				val := strings.TrimSpace(rest[colonIdx+1:])
				if val != "" {
					// inline scalar value
					item.value = unquoteYAMLValue(val)
					// Look ahead for children at deeper indentation
					cl, cb, lastIdx := collectChildren(i+1, countIndent(line))
					if len(cl) > 0 {
						children := parseYAMLNodes(cl, cb)
						item.nodes = children
					}
					if lastIdx >= i+1 {
						i = lastIdx
					}
				} else {
					// Has children — collect them
					cl, cb, lastIdx := collectChildren(i+1, countIndent(line))
					childLines = cl
					childBase = cb
					if lastIdx >= i+1 {
						i = lastIdx
					}
					if len(childLines) > 0 {
						inChild = true
					}
				}
			} else {
				item.value = unquoteYAMLValue(rest)
			}
			nodes = append(nodes, item)
		} else if strings.Contains(trimmed, ":") {
			// Mapping item
			colonIdx := strings.Index(trimmed, ":")
			key := strings.TrimSpace(trimmed[:colonIdx])
			val := strings.TrimSpace(trimmed[colonIdx+1:])

			node := yamlNode{
				key:  key,
				kind: yamlScalar,
			}

			if val == "" {
				// Could be a mapping or sequence — look ahead
				node.kind = yamlMapping
				node.value = ""
				cl, cb, lastIdx := collectChildren(i+1, countIndent(line))

				// Check if immediate children (at childBase indent) are sequence items
				hasSequenceChildren := false
				for _, clLine := range cl {
					if countIndent(clLine) != cb {
						continue // only check immediate children, not grandchildren
					}
					ct := strings.TrimSpace(clLine)
					if strings.HasPrefix(ct, "- ") || ct == "-" {
						hasSequenceChildren = true
						break
					}
				}
				if hasSequenceChildren {
					node.kind = yamlSequence
				}

				childLines = cl
				childBase = cb
				if lastIdx >= i+1 {
					i = lastIdx
				}
				if len(childLines) > 0 {
					inChild = true
				}
			} else {
				node.value = unquoteYAMLValue(val)
				// Look ahead for children at deeper indentation
				cl, cb, lastIdx := collectChildren(i+1, countIndent(line))
				if len(cl) > 0 {
					children := parseYAMLNodes(cl, cb)
					node.nodes = children
				}
				if lastIdx >= i+1 {
					i = lastIdx
				}
			}
			nodes = append(nodes, node)
		} else {
			// Plain scalar (part of a sequence)
			nodes = append(nodes, yamlNode{
				value: unquoteYAMLValue(trimmed),
				kind:  yamlScalar,
			})
		}
	}

	// Process any remaining child lines by modifying the last node in `nodes`
	if inChild && len(childLines) > 0 {
		children := parseYAMLNodes(childLines, childBase)
		if len(nodes) > 0 {
			nodes[len(nodes)-1].nodes = children
		}
	}

	return nodes
}

// countIndent counts leading spaces in a line
func countIndent(line string) int {
	count := 0
	for _, ch := range line {
		if ch == ' ' {
			count++
		} else {
			break
		}
	}
	return count
}

// ─── Schema 到 BizSystem 转换 ───

// SchemaToBizSystem 将 ConfigSchema 转换为 BizSystem
func SchemaToBizSystem(schema *types.ConfigSchema, bizID, displayName string) (*types.BizSystem, error) {
	fields := make([]types.ConfigField, len(schema.Fields))
	for i, sf := range schema.Fields {
		fields[i] = types.SchemaToConfigField(sf)
	}

	// Build domain shard from the domain field
	domainShards := []types.DomainShard{
		{
			DomainName:        schema.Domain,
			Keywords:          deriveSchemaKeywords(schema),
			DomainDescription: fmt.Sprintf("业务系统 %s 的配置域", displayName),
		},
	}

	// Group by domain (all under the same domain for schema-first)
	configsByDomain := make(map[string][]types.ConfigField)
	configsByDomain[schema.Domain] = fields

	// Build config labels
	configLabels := make(map[string]string)
	for _, sf := range schema.Fields {
		if sf.BusinessDesc != "" {
			configLabels[sf.Key] = sf.BusinessDesc
		} else if sf.UIMeta != nil && sf.UIMeta.Label != "" {
			configLabels[sf.Key] = sf.UIMeta.Label
		}
	}

	return &types.BizSystem{
		BizID:           bizID,
		DisplayName:     displayName,
		Description:     fmt.Sprintf("Schema-First 生成的业务系统: %s", displayName),
		ConfigsByDomain: configsByDomain,
		DomainShards:    domainShards,
		ConfigLabels:    configLabels,
		AllConfigs:      fields,
	}, nil
}

// deriveSchemaKeywords generates keywords from schema fields
func deriveSchemaKeywords(schema *types.ConfigSchema) []string {
	seen := make(map[string]bool)
	var keywords []string
	for _, f := range schema.Fields {
		parts := strings.Split(f.Key, ".")
		for _, p := range parts {
			if !seen[p] {
				keywords = append(keywords, p)
				seen[p] = true
			}
		}
	}
	return keywords
}

// ─── 门禁配置提取 ───

// ExtractGateConfigs 从 schema 的 contract 段提取门禁配置
func ExtractGateConfigs(schema *types.ConfigSchema) []types.GateConfig {
	var gates []types.GateConfig
	for _, f := range schema.Fields {
		if f.Contract == nil {
			continue
		}
		// RANGE gate
		if len(f.Contract.Range) == 2 {
			gates = append(gates, types.GateConfig{
				FieldKey: f.Key,
				GateType: "RANGE",
				ParamKey: "min",
				ParamVal: strconv.FormatInt(f.Contract.Range[0], 10),
			}, types.GateConfig{
				FieldKey: f.Key,
				GateType: "RANGE",
				ParamKey: "max",
				ParamVal: strconv.FormatInt(f.Contract.Range[1], 10),
			})
		}
		// DEPENDENCY gate
		for _, dep := range f.Contract.Dependencies {
			gates = append(gates, types.GateConfig{
				FieldKey: f.Key,
				GateType: "DEPENDENCY",
				ParamKey: "expression",
				ParamVal: dep,
			})
		}
		// APPROVAL gate
		if f.Contract.ApprovalRequired {
			gates = append(gates, types.GateConfig{
				FieldKey: f.Key,
				GateType: "APPROVAL",
				ParamKey: "required",
				ParamVal: "true",
			})
		}
		// SLO_WARNING gate
		if f.Contract.SLOImpact != "" {
			gates = append(gates, types.GateConfig{
				FieldKey: f.Key,
				GateType: "SLO_WARNING",
				ParamKey: "slo_impact",
				ParamVal: f.Contract.SLOImpact,
			})
		}
	}
	return gates
}

// ─── 可观测性规则提取 ───

// ExtractObservabilityRules 从 slo_impact 提取 Prometheus 告警规则
func ExtractObservabilityRules(schema *types.ConfigSchema) []string {
	var rules []string
	for _, f := range schema.Fields {
		if f.Contract != nil && f.Contract.SLOImpact != "" {
			rules = append(rules, fmt.Sprintf(
				`  - alert: %s_SLO
    expr: rate(blessstar_config_read_total{config_key=%q}[5m]) < 0.999
    for: 1m
    labels:
      severity: warning
    annotations:
      summary: "%s: %s"`,
				sanitizeAlertName(f.Key),
				f.Key,
				f.Key,
				f.Contract.SLOImpact,
			))
		}
	}
	return rules
}

// sanitizeAlertName converts a config key to a Prometheus alert name
func sanitizeAlertName(key string) string {
	s := strings.ToUpper(key)
	s = strings.NewReplacer(".", "_", "-", "_").Replace(s)
	return s
}

// ─── 边界测试生成 ───

// GenerateTestCases 从 range + dependencies 生成边界测试用例
func GenerateTestCases(schema *types.ConfigSchema) []string {
	var cases []string
	for _, f := range schema.Fields {
		if f.Contract == nil {
			continue
		}
		base := fmt.Sprintf("TestConfig_%s", sanitizeTestName(f.Key))

		// Range boundary tests
		if len(f.Contract.Range) == 2 {
			minVal := f.Contract.Range[0]
			maxVal := f.Contract.Range[1]

			cases = append(cases, fmt.Sprintf(`func %s_Boundary(t *testing.T) {
	t.Run("within_range", func(t *testing.T) {
		// %s should be within [%d, %d]
		val := int64(%d) // default or test value
		if val < %d || val > %d {
			t.Errorf("%%s value %%d out of range [%%d, %%d]", %q, val, %d, %d)
		}
	})
	t.Run("below_min", func(t *testing.T) {
		val := int64(%d - 1)
		if val >= %d {
			t.Errorf("below_min test failed: %%d should be < %%d", val, %d)
		}
	})
	t.Run("above_max", func(t *testing.T) {
		val := int64(%d + 1)
		if val <= %d {
			t.Errorf("above_max test failed: %%d should be > %%d", val, %d)
		}
	})
}`,
				base, f.Key, minVal, maxVal,
				minVal, // default within range
				minVal, maxVal,
				f.Key, minVal, maxVal,
				minVal, minVal, minVal,
				maxVal, maxVal, maxVal,
			))
		}

		// Dependency tests
		for _, dep := range f.Contract.Dependencies {
			cases = append(cases, fmt.Sprintf(`func %s_Dependency(t *testing.T) {
	t.Run(%q, func(t *testing.T) {
		// Contract dependency: %s
		t.Log("Dependency check: %s")
	})
}`,
				base, dep, dep, dep,
			))
		}
	}
	return cases
}

// ─── Bundled Schema 解析 ───

// ParseBundledSchemaYAML 解析 config-schema.bundled.yaml 文件（精简缓存格式）
// 与 ParseSchemaYAML 共享相同的解析逻辑，仅作为一个语义明确的入口点
// bundled.yaml 包含 domain / version / generated_at / generated_from / fields[]
// 其中 generated_at 和 generated_from 被现有解析器自动忽略（非 gold standard 字段）
func ParseBundledSchemaYAML(path string) (*types.ConfigSchema, error) {
	return ParseSchemaYAML(path)
}

// IsBundledSchema 快速检查是否为 bundled 格式（通过检测 generated_at 字段）
func IsBundledSchema(path string) (bool, error) {
	f, err := os.Open(path)
	if err != nil {
		return false, err
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if strings.HasPrefix(line, "generated_at:") {
			return true, nil
		}
		// Early exit: reached fields without seeing generated_at
		if strings.HasPrefix(line, "fields:") {
			return false, nil
		}
	}
	return false, scanner.Err()
}

// sanitizeTestName converts a config key to a Go test function name
func sanitizeTestName(key string) string {
	parts := strings.Split(key, ".")
	var result []string
	for _, p := range parts {
		if len(p) > 0 {
			upper := strings.ToUpper(p[:1]) + p[1:]
			result = append(result, upper)
		}
	}
	return strings.Join(result, "")
}
