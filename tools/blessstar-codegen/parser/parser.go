package parser

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/blessstar/blessstar-codegen/types"
)

// MetadataRaw maps the config_metadata.json structure directly
// (differs from ConfigField in field names and nesting)
type MetadataRaw struct {
	ConfigKey      string          `json:"config_key"`
	RegistryPath   string          `json:"registry_path"`
	DataType       string          `json:"data_type"`
	Required       bool            `json:"required"`
	DefaultValue   string          `json:"default_value"`
	BizID          string          `json:"biz_id"`
	BusinessDomain string          `json:"business_domain"`
	AIHint         string          `json:"ai_hint"`
	ValueRange     string          `json:"value_range_suggestion"`
	SearchKeywords []string        `json:"search_keywords"`
	Pattern        string          `json:"pattern,omitempty"`
	EnumValues     json.RawMessage `json:"enum_values"`
	ImpactScope    []string        `json:"impact_scope"`
	UIMetadata     UIMetadataRaw   `json:"ui_metadata"`
}

// UIMetadataRaw maps the nested ui_metadata object
type UIMetadataRaw struct {
	UIOrder int  `json:"ui_order"`
	Hidden  bool `json:"hidden"`
}

// toConfigField converts MetadataRaw to types.ConfigField
func (m *MetadataRaw) toConfigField() types.ConfigField {
	cf := types.ConfigField{
		Key:            m.ConfigKey,
		Type:           m.DataType,
		Default:        m.DefaultValue,
		Required:       m.Required,
		RegistryPath:   m.RegistryPath,
		BizID:          m.BizID,
		BusinessDomain: m.BusinessDomain,
		ValueRange:     m.ValueRange,
		UIOrder:        m.UIMetadata.UIOrder,
		Pattern:        m.Pattern,
		SearchKeywords: m.SearchKeywords,
		AIHint:         m.AIHint,
		ImpactScope:    m.ImpactScope,
	}

	// Parse enum_values if present
	if m.EnumValues != nil {
		var evs []string
		if err := json.Unmarshal(m.EnumValues, &evs); err == nil {
			cf.EnumValues = evs
		}
	}

	// Unquote default value if it's a JSON string
	if len(cf.Default) >= 2 && cf.Default[0] == '"' && cf.Default[len(cf.Default)-1] == '"' {
		if unquoted, err := strconv.Unquote(cf.Default); err == nil {
			cf.Default = unquoted
		}
	}

	return cf
}

// ParseManifest reads and parses a manifest.json file
func ParseManifest(path string) (*types.Manifest, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read manifest %s: %w", path, err)
	}
	var mf types.Manifest
	if err := json.Unmarshal(data, &mf); err != nil {
		return nil, fmt.Errorf("failed to parse manifest %s: %w", path, err)
	}
	return &mf, nil
}

// ParseMetadata reads and parses a config_metadata.json file
// The metadata JSON uses config_key/data_type/default_value and nested ui_metadata,
// which differs from the manifest's key/type/default flat structure.
func ParseMetadata(path string) ([]types.ConfigField, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read metadata %s: %w", path, err)
	}
	// config_metadata.json is an array with different field names than ConfigField
	var raws []MetadataRaw
	if err := json.Unmarshal(data, &raws); err != nil {
		return nil, fmt.Errorf("failed to parse metadata %s: %w", path, err)
	}
	fields := make([]types.ConfigField, len(raws))
	for i, r := range raws {
		fields[i] = r.toConfigField()
	}
	return fields, nil
}

// BuildBizSystem aggregates manifest and metadata into a BizSystem
func BuildBizSystem(manifestPath, metadataPath string) (*types.BizSystem, error) {
	mf, err := ParseManifest(manifestPath)
	if err != nil {
		return nil, err
	}

	metaFields, err := ParseMetadata(metadataPath)
	if err != nil {
		// metadata is optional; use manifest fields
		metaFields = nil
	}

	// Index metadata by config_key
	metaByKey := make(map[string]types.ConfigField)
	for _, f := range metaFields {
		metaByKey[f.Key] = f
	}

	// Merge manifest fields with metadata enrichment
	allConfigs := make([]types.ConfigField, len(mf.Fields))
	for i, f := range mf.Fields {
		enriched := f
		if meta, ok := metaByKey[f.Key]; ok {
			enriched.RegistryPath = meta.RegistryPath
			enriched.BizID = meta.BizID
			enriched.BusinessDomain = meta.BusinessDomain
			if meta.ValueRange != "" {
				enriched.ValueRange = meta.ValueRange
			}
			enriched.UIOrder = meta.UIOrder
			enriched.EnumValues = meta.EnumValues
			enriched.Pattern = meta.Pattern
			enriched.SearchKeywords = meta.SearchKeywords
			enriched.AIHint = meta.AIHint
			enriched.ImpactScope = meta.ImpactScope
			enriched.Default = meta.Default
		}
		allConfigs[i] = enriched
	}

	// Build domain shards from ai_data.config_domains
	domainShards := buildDomainShards(mf)

	// Group configs by business domain
	configsByDomain := make(map[string][]types.ConfigField)
	for _, c := range allConfigs {
		domain := c.BusinessDomain
		if domain == "" {
			// Try to infer domain from key prefix via domain shards
			domain = inferDomain(c.Key, domainShards)
		}
		configsByDomain[domain] = append(configsByDomain[domain], c)
	}

	// Sort configs within each domain by UIOrder
	for _, configs := range configsByDomain {
		sort.Slice(configs, func(i, j int) bool {
			return configs[i].UIOrder < configs[j].UIOrder
		})
	}

	// Also compute per-domainshard config keys for the shards
	domainKeyMap := make(map[string][]string)
	for _, c := range allConfigs {
		domain := c.BusinessDomain
		if domain == "" {
			domain = inferDomain(c.Key, domainShards)
		}
		domainKeyMap[domain] = append(domainKeyMap[domain], c.Key)
	}
	for i := range domainShards {
		if keys, ok := domainKeyMap[domainShards[i].DomainName]; ok {
			domainShards[i].ConfigKeys = keys
		}
	}

	return &types.BizSystem{
		BizID:           mf.BizID,
		DisplayName:     mf.DisplayName,
		Description:     mf.Description,
		ConfigsByDomain: configsByDomain,
		DomainShards:    domainShards,
		ConfigLabels:    mf.AIData.ConfigLabels,
		AllConfigs:      allConfigs,
	}, nil
}

// inferDomain tries to find which domain a config key belongs to
func inferDomain(key string, shards []types.DomainShard) string {
	parts := strings.Split(strings.ToLower(key), ".")
	for _, shard := range shards {
		for _, kw := range shard.Keywords {
			for _, p := range parts {
				if p == strings.ToLower(kw) {
					return shard.DomainName
				}
			}
		}
	}
	return "未分类"
}

// buildDomainShards constructs DomainShard objects from manifest config_domains
func buildDomainShards(mf *types.Manifest) []types.DomainShard {
	var shards []types.DomainShard
	for domainName, desc := range mf.AIData.ConfigDomains {
		keywords := deriveKeywords(domainName, mf.Fields)
		shards = append(shards, types.DomainShard{
			DomainName:        domainName,
			Keywords:          keywords,
			DomainDescription: desc,
		})
	}
	// Sort for deterministic output
	sort.Slice(shards, func(i, j int) bool {
		return shards[i].DomainName < shards[j].DomainName
	})
	return shards
}

// deriveKeywords generates search keywords for a domain based on config key prefixes
func deriveKeywords(domainName string, fields []types.ConfigField) []string {
	seen := make(map[string]bool)
	var keywords []string

	// Collect unique key parts that match the domain
	for _, f := range fields {
		if strings.HasPrefix(f.Key, domainName+".") {
			parts := strings.Split(f.Key, ".")
			// The domain name itself is a keyword
			if !seen[domainName] {
				keywords = append(keywords, domainName)
				seen[domainName] = true
			}
			// The next level after domain is also a keyword
			for i, p := range parts {
				if i > 0 && !seen[p] {
					keywords = append(keywords, p)
					seen[p] = true
				}
			}
		}
	}

	// If no fields matched, use domain name parts
	if len(keywords) == 0 {
		for _, part := range strings.Split(domainName, "_") {
			if !seen[part] {
				keywords = append(keywords, part)
				seen[part] = true
			}
		}
	}

	sort.Strings(keywords)
	return keywords
}

// DetectConfigDir finds the biz-registry directory relative to the manifests
func DetectConfigDir(manifestPath string) string {
	return filepath.Dir(manifestPath)
}

// OutputFileFromTemplate generates an output file path from template and key
func OutputFileFromTemplate(outputDir, subDir, fileName string) string {
	dir := filepath.Join(outputDir, subDir)
	return filepath.Join(dir, fileName)
}

// ValidationError represents a consistency check failure
type ValidationError struct {
	Field   string
	Message string
}

func (e ValidationError) Error() string {
	return e.Message
}

// ValidationResult holds the results of manifest-metadata validation.
type ValidationResult struct {
	Errors   []ValidationError
	Warnings []string
}

// ValidateConsistency checks that manifest.json and config_metadata.json are consistent.
// Returns nil if metadata path is empty (optional metadata).
func ValidateConsistency(manifestPath, metadataPath string) (*ValidationResult, error) {
	result := &ValidationResult{}

	if metadataPath == "" {
		return result, nil // metadata is optional
	}

	mf, err := ParseManifest(manifestPath)
	if err != nil {
		return nil, fmt.Errorf("failed to parse manifest for validation: %w", err)
	}

	metaFields, err := ParseMetadata(metadataPath)
	if err != nil {
		return nil, fmt.Errorf("failed to parse metadata for validation: %w", err)
	}

	// Index manifest fields by key
	manifestByKey := make(map[string]types.ConfigField)
	for _, f := range mf.Fields {
		manifestByKey[f.Key] = f
	}

	// Index metadata fields by config_key
	metaByKey := make(map[string]types.ConfigField)
	for _, f := range metaFields {
		metaByKey[f.Key] = f
	}

	// Check 1: Every field in manifest must have a corresponding metadata entry
	for _, f := range mf.Fields {
		meta, ok := metaByKey[f.Key]
		if !ok {
			result.Errors = append(result.Errors, ValidationError{
				Field:   f.Key,
				Message: fmt.Sprintf("manifest field %q has no corresponding entry in config_metadata.json", f.Key),
			})
			continue
		}

		// Check type consistency
		if meta.Type != "" && f.Type != "" && meta.Type != f.Type {
			result.Errors = append(result.Errors, ValidationError{
				Field:   f.Key,
				Message: fmt.Sprintf("type mismatch for %q: manifest says %q, metadata says %q", f.Key, f.Type, meta.Type),
			})
		}

		// Check business_domain is assigned
		if meta.BusinessDomain == "" {
			result.Warnings = append(result.Warnings,
				fmt.Sprintf("metadata entry %q has no business_domain assigned; will be inferred from key prefix", f.Key))
		}

		// Check registry_path is assigned
		if meta.RegistryPath == "" {
			result.Warnings = append(result.Warnings,
				fmt.Sprintf("metadata entry %q has no registry_path; will use auto-generated path", f.Key))
		}
	}

	// Check 2: Every metadata entry should have a corresponding manifest field
	for _, f := range metaFields {
		if _, ok := manifestByKey[f.Key]; !ok {
			result.Errors = append(result.Errors, ValidationError{
				Field:   f.Key,
				Message: fmt.Sprintf("config_metadata.json has entry %q but manifest.json has no such field", f.Key),
			})
		}
	}

	return result, nil
}

// ValidateConsistencyOrExit calls ValidateConsistency and exits on errors.
// Returns true if validation passed (no errors), false if there were errors.
func ValidateConsistencyOrExit(manifestPath, metadataPath string) bool {
	result, err := ValidateConsistency(manifestPath, metadataPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "❌ Validation error: %v\n", err)
		return false
	}

	if len(result.Errors) > 0 {
		fmt.Fprintf(os.Stderr, "❌ manifest.json 与 config_metadata.json 存在 %d 个不一致:\n", len(result.Errors))
		for _, e := range result.Errors {
			fmt.Fprintf(os.Stderr, "   - %s\n", e.Message)
		}
		return false
	}

	if len(result.Warnings) > 0 {
		fmt.Printf("⚠️  发现 %d 个可优化项:\n", len(result.Warnings))
		for _, w := range result.Warnings {
			fmt.Printf("   - %s\n", w)
		}
	}

	return true
}

// errGroup holds os import needed above
var _ = os.Stderr
