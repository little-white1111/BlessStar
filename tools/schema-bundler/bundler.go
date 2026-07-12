package main

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

// ─── 数据类型 ───

// BundledField 表示 bundled.yaml 中单个配置字段（精简版）
// 仅含 key / type / default / contract，不含 ui_meta / search_keywords 等
type BundledField struct {
	Key     string       `yaml:"key"`
	Type    string       `yaml:"type"`
	Default string       `yaml:"default"`
	Contract *BundledContract `yaml:"contract,omitempty"`
}

// BundledContract 表示字段的契约规则（精简版）
type BundledContract struct {
	Range            []float64 `yaml:"range,omitempty"`
	Dependencies     []string  `yaml:"dependencies,omitempty"`
	SLOImpact        string    `yaml:"slo_impact,omitempty"`
	ApprovalRequired bool      `yaml:"approval_required,omitempty"`
	Immutable        bool      `yaml:"immutable,omitempty"`
}

// BundledSchema 表示 config-schema.bundled.yaml 的顶层结构
type BundledSchema struct {
	Domain        string         `yaml:"domain"`
	Version       string         `yaml:"version"`
	GeneratedAt   string         `yaml:"generated_at"`
	GeneratedFrom []string       `yaml:"generated_from"`
	Fields        []BundledField `yaml:"fields"`
}

// parsedSchema 表示单个 SSOT 文件的解析结果
type parsedSchema struct {
	FilePath string
	Domain   string
	Version  string
	Fields   []BundledField
	Sources  map[string]string // field key -> source file name for conflict detection
}

// ─── 目录扫描 ───

func scanYAMLFiles(dir string) ([]string, error) {
	entries, err := os.ReadDir(dir)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, fmt.Errorf("directory %s does not exist", dir)
		}
		return nil, fmt.Errorf("failed to read directory %s: %w", dir, err)
	}

	var files []string
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		name := entry.Name()
		if strings.HasSuffix(name, ".yaml") || strings.HasSuffix(name, ".yml") {
			files = append(files, name)
		}
	}
	sort.Strings(files) // deterministic order
	return files, nil
}

// ─── 逐文件解析 ───

func parseAllSchemas(dir string, fileNames []string) ([]*parsedSchema, error) {
	var schemas []*parsedSchema
	for _, name := range fileNames {
		path := filepath.Join(dir, name)
		data, err := os.ReadFile(path)
		if err != nil {
			return nil, fmt.Errorf("failed to read %s: %w", path, err)
		}

		schema, err := parseSSOTFile(data, name)
		if err != nil {
			return nil, fmt.Errorf("failed to parse %s: %w", name, err)
		}
		schemas = append(schemas, schema)
	}
	return schemas, nil
}

// parseSSOTFile 解析单个 SSOT YAML 文件，提取 bundled 所需的字段
func parseSSOTFile(data []byte, fileName string) (*parsedSchema, error) {
	ps := &parsedSchema{
		FilePath: fileName,
		Sources:  make(map[string]string),
	}

	lines := strings.Split(string(data), "\n")
	var inFields bool
	var inDepsBlock bool // are we inside a dependencies: block?
	var currentField *BundledField

	for _, line := range lines {
		trimmed := strings.TrimSpace(line)

		// Skip comments and blanks
		if trimmed == "" || strings.HasPrefix(trimmed, "#") {
			continue
		}

		// Top-level keys
		if !inFields {
			if strings.HasPrefix(trimmed, "domain:") {
				ps.Domain = strings.TrimSpace(strings.TrimPrefix(trimmed, "domain:"))
				continue
			}
			if strings.HasPrefix(trimmed, "version:") {
				ps.Version = strings.TrimSpace(strings.TrimPrefix(trimmed, "version:"))
				continue
			}
			if trimmed == "fields:" {
				inFields = true
				continue
			}
			continue
		}

		// Inside fields: detect list item start "- key:"
		if strings.HasPrefix(trimmed, "- key:") {
			// Save previous field
			if currentField != nil {
				ps.Fields = append(ps.Fields, *currentField)
				ps.Sources[currentField.Key] = fileName
			}
			currentField = &BundledField{
				Key: strings.TrimSpace(strings.TrimPrefix(trimmed, "- key:")),
			}
			continue
		}

		if currentField == nil {
			continue
		}

		// Parse field sub-keys (skip ui_meta, search_keywords, ai_hint, impact_scope, env_overrides)
		switch {
		case strings.HasPrefix(trimmed, "type:"):
			currentField.Type = strings.TrimSpace(strings.TrimPrefix(trimmed, "type:"))

		case strings.HasPrefix(trimmed, "default:"):
			currentField.Default = strings.TrimSpace(strings.TrimPrefix(trimmed, "default:"))

		case strings.HasPrefix(trimmed, "contract:"):
			if currentField.Contract == nil {
				currentField.Contract = &BundledContract{}
			}
			// Contract block is handled further below

		case strings.HasPrefix(trimmed, "business_desc:"):
			// Skip: not included in bundled
			continue

		case strings.HasPrefix(trimmed, "ui_meta:"):
			// Skip: not included in bundled
			continue

		case strings.HasPrefix(trimmed, "search_keywords:"):
			// Skip: not included in bundled
			continue

		case strings.HasPrefix(trimmed, "ai_hint:"):
			// Skip: not included in bundled
			continue

		case strings.HasPrefix(trimmed, "impact_scope:"):
			// Skip: not included in bundled
			continue

		case strings.HasPrefix(trimmed, "env_overrides:"):
			// Skip: not included in bundled
			continue

		case trimmed == "required:":
			// Skip: not included in bundled
			continue
		}

		// Contract sub-fields (indented under contract:)
		if currentField.Contract != nil {
			switch {
			case strings.HasPrefix(trimmed, "range:"):
				// Parse range: [min, max]
				rangeStr := strings.TrimSpace(strings.TrimPrefix(trimmed, "range:"))
				currentField.Contract.Range = parseRange(rangeStr)

			case strings.HasPrefix(trimmed, "slo_impact:"):
				currentField.Contract.SLOImpact = stripQuotes(strings.TrimSpace(strings.TrimPrefix(trimmed, "slo_impact:")))

			case strings.HasPrefix(trimmed, "approval_required:"):
				val := strings.TrimSpace(strings.TrimPrefix(trimmed, "approval_required:"))
				currentField.Contract.ApprovalRequired = val == "true"

			case strings.HasPrefix(trimmed, "immutable:"):
				val := strings.TrimSpace(strings.TrimPrefix(trimmed, "immutable:"))
				currentField.Contract.Immutable = val == "true"

			case strings.HasPrefix(trimmed, "dependencies:"):
				inDepsBlock = true
				if currentField.Contract.Dependencies == nil {
					currentField.Contract.Dependencies = make([]string, 0)
				}
			}
		}

		// Parse dependency list items: `  - "expr"`
		// Note: inDepsBlock is set true on the same line as "dependencies:",
		// so we must NOT exit the block for that line.
		if inDepsBlock && strings.HasPrefix(trimmed, "dependencies:") {
			// The line that activated the deps block — stay in block for next line.
		} else if inDepsBlock {
			if strings.HasPrefix(trimmed, "- ") && !strings.HasPrefix(trimmed, "- key:") {
				dep := strings.TrimSpace(strings.TrimPrefix(trimmed, "- "))
				dep = stripQuotes(dep)
				currentField.Contract.Dependencies = append(currentField.Contract.Dependencies, dep)
			} else if !strings.HasPrefix(trimmed, "- ") {
				// Line outside deps block ends it
				inDepsBlock = false
			}
		}
	}

	// Append last field
	if currentField != nil {
		ps.Fields = append(ps.Fields, *currentField)
		ps.Sources[currentField.Key] = fileName
	}

	return ps, nil
}

// parseRange 解析 "[min, max]" 格式的字符串
func parseRange(s string) []float64 {
	s = strings.TrimSpace(s)
	s = strings.TrimPrefix(s, "[")
	s = strings.TrimSuffix(s, "]")
	parts := strings.Split(s, ",")
	if len(parts) != 2 {
		return nil
	}
	var result []float64
	for _, p := range parts {
		var v float64
		if _, err := fmt.Sscanf(strings.TrimSpace(p), "%f", &v); err != nil {
			return nil
		}
		result = append(result, v)
	}
	return result
}

// ─── 合并 ───

func mergeSchemas(schemas []*parsedSchema) (*BundledSchema, error) {
	if len(schemas) == 0 {
		return nil, fmt.Errorf("no schemas to merge")
	}

	domain := schemas[0].Domain
	version := schemas[0].Version
	seen := make(map[string]bool) // track field keys for duplicate detection
	var fields []BundledField
	var generatedFrom []string

	for _, s := range schemas {
		// Verify all schemas share the same domain
		if s.Domain != domain {
			return nil, fmt.Errorf("domain mismatch: %s in %s vs %s", s.Domain, s.FilePath, domain)
		}
		// Collect version from first file only (they should match)
		generatedFrom = append(generatedFrom, s.FilePath)

		for _, f := range s.Fields {
			if seen[f.Key] {
				return nil, fmt.Errorf("duplicate field key '%s' found in %s", f.Key, s.FilePath)
			}
			seen[f.Key] = true
			fields = append(fields, f)
		}
	}

	// Sort fields by key for deterministic output
	sort.Slice(fields, func(i, j int) bool {
		return fields[i].Key < fields[j].Key
	})

	return &BundledSchema{
		Domain:        domain,
		Version:       version,
		GeneratedAt:   time.Now().UTC().Format(time.RFC3339),
		GeneratedFrom: generatedFrom,
		Fields:        fields,
	}, nil
}

// writeBundledYAML 将合并结果写为 bundled YAML
func writeBundledYAML(schema *BundledSchema, outputPath string) error {
	var b strings.Builder

	b.WriteString("# config-schema.bundled.yaml — 编译期聚合缓存（自动生成，禁止手动编辑）\n")
	b.WriteString(fmt.Sprintf("# Generated at: %s\n", schema.GeneratedAt))
	b.WriteString(fmt.Sprintf("# From: %s\n\n", strings.Join(schema.GeneratedFrom, ", ")))
	b.WriteString(fmt.Sprintf("domain: %s\n", schema.Domain))
	b.WriteString(fmt.Sprintf("version: %s\n", schema.Version))
	b.WriteString(fmt.Sprintf("generated_at: \"%s\"\n", schema.GeneratedAt))
	b.WriteString("generated_from:\n")
	for _, f := range schema.GeneratedFrom {
		b.WriteString(fmt.Sprintf("  - %s\n", f))
	}
	b.WriteString("\nfields:\n")

	for _, field := range schema.Fields {
		b.WriteString(fmt.Sprintf("  - key: %s\n", field.Key))
		b.WriteString(fmt.Sprintf("    type: %s\n", field.Type))
		b.WriteString(fmt.Sprintf("    default: %s\n", field.Default))

		if field.Contract != nil && hasContractContent(field.Contract) {
			b.WriteString("    contract:\n")
			if len(field.Contract.Range) == 2 {
				b.WriteString(fmt.Sprintf("      range: [%g, %g]\n", field.Contract.Range[0], field.Contract.Range[1]))
			}
			if len(field.Contract.Dependencies) > 0 {
				b.WriteString("      dependencies:\n")
				for _, dep := range field.Contract.Dependencies {
					b.WriteString(fmt.Sprintf("        - \"%s\"\n", dep))
				}
			}
			if field.Contract.SLOImpact != "" {
				b.WriteString(fmt.Sprintf("      slo_impact: %s\n", field.Contract.SLOImpact))
			}
			if field.Contract.ApprovalRequired {
				b.WriteString("      approval_required: true\n")
			}
			if field.Contract.Immutable {
				b.WriteString("      immutable: true\n")
			}
		}
	}

	return os.WriteFile(outputPath, []byte(b.String()), 0644)
}

// stripQuotes 去除字符串首尾的引号（单引号或双引号）
func stripQuotes(s string) string {
	if len(s) < 2 {
		return s
	}
	if (s[0] == '"' && s[len(s)-1] == '"') || (s[0] == '\'' && s[len(s)-1] == '\'') {
		return s[1 : len(s)-1]
	}
	return s
}

// hasContractContent 检查 contract 中是否有实际内容
func hasContractContent(c *BundledContract) bool {
	return len(c.Range) == 2 || len(c.Dependencies) > 0 || c.SLOImpact != "" || c.ApprovalRequired || c.Immutable
}
