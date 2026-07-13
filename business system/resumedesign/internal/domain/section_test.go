package domain

import "testing"

func TestIsValidSectionType(t *testing.T) {
	tests := []struct {
		name  string
		st    SectionType
		valid bool
	}{
		{"personal", SectionPersonal, true},
		{"education", SectionEducation, true},
		{"work", SectionWork, true},
		{"project", SectionProject, true},
		{"skill", SectionSkill, true},
		{"award", SectionAward, true},
		{"invalid", "unknown", false},
		{"empty", SectionType(""), false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidSectionType(tt.st); got != tt.valid {
				t.Errorf("IsValidSectionType(%q) = %v, want %v", tt.st, got, tt.valid)
			}
		})
	}
}

func TestValidSectionTypes(t *testing.T) {
	types := ValidSectionTypes()
	if len(types) != 6 {
		t.Errorf("ValidSectionTypes() returned %d types, want 6", len(types))
	}

	// 验证每个类型都有效
	for _, st := range types {
		if !IsValidSectionType(st) {
			t.Errorf("ValidSectionTypes() contains invalid type %q", st)
		}
	}
}
