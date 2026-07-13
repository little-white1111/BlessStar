package domain

import "testing"

func TestIsValidSessionStatus(t *testing.T) {
	tests := []struct {
		name  string
		ss    SessionStatus
		valid bool
	}{
		{"draft", SessionDraft, true},
		{"processing", SessionProcessing, true},
		{"completed", SessionCompleted, true},
		{"failed", SessionFailed, true},
		{"invalid", "unknown", false},
		{"empty", SessionStatus(""), false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidSessionStatus(tt.ss); got != tt.valid {
				t.Errorf("IsValidSessionStatus(%q) = %v, want %v", tt.ss, got, tt.valid)
			}
		})
	}
}

func TestRewriteSessionDefaultValues(t *testing.T) {
	session := &RewriteSession{
		ID:       "test-id",
		ResumeID: "resume-1",
		JdID:     "jd-1",
		Status:   SessionDraft,
	}

	if session.Status != SessionDraft {
		t.Errorf("expected draft status, got %s", session.Status)
	}
	if session.ID != "test-id" {
		t.Errorf("expected test-id, got %s", session.ID)
	}
}
