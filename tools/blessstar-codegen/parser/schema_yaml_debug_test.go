package parser

import (
	"testing"
)

func TestDebugParseSchemaYAML(t *testing.T) {
	schema, err := ParseSchemaYAML("c:\\Users\\LJHlj\\blessstar\\BlessStar\\business system\\douyin-mall\\config-schema.yaml")
	if err != nil {
		t.Fatalf("ParseSchemaYAML failed: %v", err)
	}
	t.Logf("Domain: %q", schema.Domain)
	t.Logf("Version: %q", schema.Version)
	t.Logf("Fields count: %d", len(schema.Fields))
	for i, f := range schema.Fields {
		t.Logf("  Field[%d]: key=%q type=%q default=%q", i, f.Key, f.Type, f.Default)
		if f.Contract != nil {
			t.Logf("    Contract: range=%v, deps=%v, slo=%q, approval=%v",
				f.Contract.Range, f.Contract.Dependencies, f.Contract.SLOImpact, f.Contract.ApprovalRequired)
		}
		if f.UIMeta != nil {
			t.Logf("    UIMeta: label=%q, order=%d", f.UIMeta.Label, f.UIMeta.Order)
		}
	}
}
