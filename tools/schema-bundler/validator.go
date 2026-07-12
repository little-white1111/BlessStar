package main

import (
	"fmt"
)

// ─── Gold Standard 字段定义 ───

// goldFieldDef 定义金标准认可的字段
type goldFieldDef struct {
	QualifiedName string // 如 "key", "type", "contract.range"
	Required      bool   // true=缺失视为非法
}

// BS_GOLD_STANDARD_V1 金标准 v1 表
// 定义 SchemaLoader 认可的字段集合
var BS_GOLD_STANDARD_V1 = []goldFieldDef{
	{QualifiedName: "key", Required: true},
	{QualifiedName: "type", Required: true},
	{QualifiedName: "default", Required: false},
	{QualifiedName: "contract.range", Required: false},
	{QualifiedName: "contract.dependencies", Required: false},
	{QualifiedName: "contract.slo_impact", Required: false},
	{QualifiedName: "contract.approval_required", Required: false},
}

// ─── 每文件 Gold Standard 校验 ───

func validateEachSchema(schemas []*parsedSchema) error {
	for _, s := range schemas {
		if err := validateSingleSchema(s); err != nil {
			return fmt.Errorf("file %s: %w", s.FilePath, err)
		}
	}
	return nil
}

func validateSingleSchema(s *parsedSchema) error {
	// 校验 domain 不为空
	if s.Domain == "" {
		return fmt.Errorf("domain is required")
	}
	// 校验 version 不为空
	if s.Version == "" {
		return fmt.Errorf("version is required")
	}
	// 校验每个字段
	for _, f := range s.Fields {
		if f.Key == "" {
			return fmt.Errorf("field key is required")
		}
		if f.Type == "" {
			return fmt.Errorf("field '%s': type is required", f.Key)
		}
	}
	return nil
}

// ─── 跨文件依赖解析 ───

func resolveCrossFileDeps(schemas []*parsedSchema) error {
	// Build global field index
	globalFields := make(map[string]string) // key -> source file
	for _, s := range schemas {
		for _, f := range s.Fields {
			globalFields[f.Key] = s.FilePath
		}
	}

	// Check all dependencies reference valid fields
	for _, s := range schemas {
		for _, f := range s.Fields {
			if f.Contract == nil {
				continue
			}
			for _, dep := range f.Contract.Dependencies {
				// Extract referenced field keys from dependency expressions
				refs := extractFieldRefs(dep)
				for _, ref := range refs {
					if _, exists := globalFields[ref]; !exists {
						return fmt.Errorf(
							"file %s: field '%s' depends on '%s' which does not exist in any SSOT file",
							s.FilePath, f.Key, ref,
						)
					}
				}
			}
		}
	}
	return nil
}

// extractFieldRefs 从依赖表达式中提取引用的字段名
// 例如 "payment.timeout > payment.max_retry * 2" → ["payment.timeout", "payment.max_retry"]
func extractFieldRefs(expr string) []string {
	var refs []string
	current := make([]rune, 0)
	inRef := false

	for _, ch := range expr {
		if ch == ' ' || ch == '\t' {
			if inRef {
				ref := string(current)
				if ref != "" && !isNumericOnly(ref) {
					refs = append(refs, ref)
				}
				current = current[:0]
				inRef = false
			}
			continue
		}
		if ch == '>' || ch == '<' || ch == '=' || ch == '!' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '(' || ch == ')' || ch == ',' {
			if inRef {
				ref := string(current)
				if ref != "" && !isNumericOnly(ref) {
					refs = append(refs, ref)
				}
				current = current[:0]
				inRef = false
			}
			continue
		}
		if ch == '.' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' {
			current = append(current, ch)
			inRef = true
		}
	}

	// Last token
	if inRef {
		ref := string(current)
		if ref != "" && !isNumericOnly(ref) {
			refs = append(refs, ref)
		}
	}

	return refs
}

// isNumericOnly 检查 token 是否仅由数字字符组成（允许可选的负号和小数点）
func isNumericOnly(s string) bool {
	if len(s) == 0 {
		return false
	}
	hasDigit := false
	for i, ch := range s {
		if ch == '-' && i == 0 {
			continue
		}
		if ch == '.' {
			continue
		}
		if ch >= '0' && ch <= '9' {
			hasDigit = true
			continue
		}
		return false
	}
	return hasDigit
}

// ─── 全局 Gold Standard 校验 ───

func validateBundled(bundled *BundledSchema) error {
	if bundled.Domain == "" {
		return fmt.Errorf("bundled schema: domain is required")
	}
	if bundled.Version == "" {
		return fmt.Errorf("bundled schema: version is required")
	}
	if len(bundled.Fields) == 0 {
		return fmt.Errorf("bundled schema: at least one field is required")
	}

	// 全局字段去重已在 merge 阶段完成，但需要确认没有空 key
	for _, f := range bundled.Fields {
		if f.Key == "" {
			return fmt.Errorf("bundled schema: field with empty key found")
		}
		if f.Type == "" {
			return fmt.Errorf("bundled schema: field '%s' has empty type", f.Key)
		}
	}

	return nil
}
