package parser

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/blessstar/blessstar-codegen/types"
)

const testManifest = `{
  "biz_id": "douyin-mall",
  "display_name": "抖音商城",
  "description": "测试业务系统",
  "version": "1.0.0",
  "sdk_version": ">=1.0.0",
  "fields": [
    {"key": "auth.jwt.token_expiry_seconds", "type": "I64", "default": "86400", "description": "JWT过期时间", "required": false},
    {"key": "user.role.values", "type": "ENUM", "default": "[\"user\",\"admin\"]", "description": "用户角色", "required": false}
  ],
  "ai_data": {
    "summary": "测试摘要",
    "business_capabilities": ["测试能力"],
    "config_domains": {
      "auth": "认证鉴权域",
      "user": "用户域"
    },
    "configLabels": {
      "auth.jwt.token_expiry_seconds": "JWT过期时间",
      "user.role.values": "用户角色"
    },
    "invertedIndex": [],
    "skillRoutes": []
  }
}`

const testMetadata = `[
  {
    "config_key": "auth.jwt.token_expiry_seconds",
    "registry_path": "/config/douyin-mall/auth/jwt/token_expiry_seconds",
    "data_type": "I64",
    "required": false,
    "default_value": "86400",
    "biz_id": "douyin-mall",
    "business_domain": "认证鉴权",
    "ai_hint": "JWT令牌过期时间配置",
    "value_range_suggestion": "3600~604800",
    "ui_metadata": {"ui_label": "Token过期时间（秒）", "ui_description": "用户登录后JWT令牌的有效期", "ui_placeholder": "输入过期秒数", "ui_order": 1, "hidden": false},
    "search_keywords": ["token过期", "登录有效期"],
    "pattern": "^[0-9]+$",
    "enum_values": null,
    "impact_scope": ["用户登录", "会话管理", "接口鉴权"],
    "registration_phase": "P1"
  },
  {
    "config_key": "user.role.values",
    "registry_path": "/config/douyin-mall/user/role/values",
    "data_type": "ENUM",
    "required": false,
    "default_value": "[\"user\",\"admin\"]",
    "biz_id": "douyin-mall",
    "business_domain": "用户管理",
    "ai_hint": "用户角色枚举",
    "value_range_suggestion": "user, admin",
    "ui_metadata": {"ui_label": "用户角色枚举", "ui_description": "系统支持的用户角色列表", "ui_placeholder": "输入角色名称列表", "ui_order": 3, "hidden": false},
    "search_keywords": ["用户角色"],
    "enum_values": ["user", "admin"],
    "impact_scope": ["用户注册", "权限控制"],
    "registration_phase": "P1"
  }
]`

func writeTestFile(t *testing.T, dir, name, content string) string {
	t.Helper()
	path := filepath.Join(dir, name)
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatalf("Failed to write %s: %v", path, err)
	}
	return path
}

func TestParseManifest(t *testing.T) {
	dir := t.TempDir()
	manifestPath := writeTestFile(t, dir, "manifest.json", testManifest)

	mf, err := ParseManifest(manifestPath)
	if err != nil {
		t.Fatalf("ParseManifest failed: %v", err)
	}

	if mf.BizID != "douyin-mall" {
		t.Errorf("BizID = %q, want %q", mf.BizID, "douyin-mall")
	}
	if len(mf.Fields) != 2 {
		t.Errorf("Fields count = %d, want 2", len(mf.Fields))
	}
	if mf.Fields[0].Key != "auth.jwt.token_expiry_seconds" {
		t.Errorf("First field key = %q, want %q", mf.Fields[0].Key, "auth.jwt.token_expiry_seconds")
	}
	if mf.Fields[0].Type != "I64" {
		t.Errorf("First field type = %q, want %q", mf.Fields[0].Type, "I64")
	}
}

func TestParseMetadata(t *testing.T) {
	dir := t.TempDir()
	metaPath := writeTestFile(t, dir, "config_metadata.json", testMetadata)

	fields, err := ParseMetadata(metaPath)
	if err != nil {
		t.Fatalf("ParseMetadata failed: %v", err)
	}

	if len(fields) != 2 {
		t.Errorf("Meta fields count = %d, want 2", len(fields))
	}
	if fields[0].BusinessDomain != "认证鉴权" {
		t.Errorf("BusinessDomain = %q, want %q", fields[0].BusinessDomain, "认证鉴权")
	}
	if fields[0].AIHint != "JWT令牌过期时间配置" {
		t.Errorf("AIHint = %q, want %q", fields[0].AIHint, "JWT令牌过期时间配置")
	}
}

func TestBuildBizSystem(t *testing.T) {
	dir := t.TempDir()
	manifestPath := writeTestFile(t, dir, "manifest.json", testManifest)
	metaPath := writeTestFile(t, dir, "config_metadata.json", testMetadata)

	biz, err := BuildBizSystem(manifestPath, metaPath)
	if err != nil {
		t.Fatalf("BuildBizSystem failed: %v", err)
	}

	if len(biz.AllConfigs) != 2 {
		t.Errorf("AllConfigs count = %d, want 2", len(biz.AllConfigs))
	}
	if len(biz.DomainShards) != 2 {
		t.Errorf("DomainShards count = %d, want 2", len(biz.DomainShards))
	}
	if len(biz.ConfigsByDomain) < 1 {
		t.Error("ConfigsByDomain is empty")
	}

	// Verify enrichment: metadata fields should be applied
	for _, c := range biz.AllConfigs {
		if c.Key == "auth.jwt.token_expiry_seconds" && c.AIHint != "JWT令牌过期时间配置" {
			t.Errorf("Expected enriched AIHint for auth field, got %q", c.AIHint)
		}
	}
}

func TestInferDomain(t *testing.T) {
	shards := []types.DomainShard{
		{DomainName: "认证鉴权", Keywords: []string{"auth", "jwt", "token", "password", "bcrypt"}},
		{DomainName: "用户管理", Keywords: []string{"user", "registration", "validation", "role", "status"}},
	}

	tests := []struct {
		name     string
		key      string
		shards   []types.DomainShard
		expected string
	}{
		{"auth key with shards", "auth.jwt.token_expiry_seconds", shards, "认证鉴权"},
		{"user key with shards", "user.role.values", shards, "用户管理"},
		{"unknown key with shards", "unknown.key", shards, "未分类"},
		{"no shards falls back", "auth.jwt.key", nil, "未分类"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := inferDomain(tt.key, tt.shards)
			if got != tt.expected {
				t.Errorf("inferDomain(%q) = %q, want %q", tt.key, got, tt.expected)
			}
		})
	}
}

func TestDetectConfigDir(t *testing.T) {
	input := "/path/to/biz-registry/douyin-mall/manifest.json"
	dir := DetectConfigDir(input)
	// Use filepath.ToSlash to normalize Windows backslashes
	normalized := strings.ReplaceAll(dir, "\\", "/")
	if normalized != "/path/to/biz-registry/douyin-mall" {
		t.Errorf("DetectConfigDir(%q) = %q, want %q", input, dir, "/path/to/biz-registry/douyin-mall")
	}
}

func TestBuildDomainShards(t *testing.T) {
	dir := t.TempDir()
	manifestPath := writeTestFile(t, dir, "manifest.json", testManifest)

	mf, err := ParseManifest(manifestPath)
	if err != nil {
		t.Fatalf("ParseManifest failed: %v", err)
	}

	shards := buildDomainShards(mf)
	if len(shards) != 2 {
		t.Errorf("buildDomainShards returned %d shards, want 2", len(shards))
	}
}

func TestParseNonExistentFile(t *testing.T) {
	_, err := ParseManifest("/nonexistent/manifest.json")
	if err == nil {
		t.Error("Expected error for nonexistent manifest, got nil")
	}
}
