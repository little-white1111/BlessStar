package domain

import "testing"

func TestIsValidChangeType(t *testing.T) {
	tests := []struct {
		name  string
		ct    ChangeType
		valid bool
	}{
		{"modified", ChangeModified, true},
		{"added", ChangeAdded, true},
		{"removed", ChangeRemoved, true},
		{"unchanged", ChangeUnchanged, true},
		{"invalid", "unknown", false},
		{"empty", ChangeType(""), false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidChangeType(tt.ct); got != tt.valid {
				t.Errorf("IsValidChangeType(%q) = %v, want %v", tt.ct, got, tt.valid)
			}
		})
	}
}
