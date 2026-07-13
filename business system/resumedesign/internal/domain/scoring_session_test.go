package domain

import (
	"testing"
	"time"
)

func TestJDScoringSession_AddResult_Passed(t *testing.T) {
	session := &JDScoringSession{
		ID:             "session-1",
		Keywords:       []string{"Go"},
		TrustThreshold: 0.6,
		MatchThreshold: 0.6,
		Status:         ScoringPending,
		CreatedAt:      time.Now(),
	}

	jd := &ScoredJD{
		ID:         "jd-1",
		TrustScore: 0.8,
		MatchScore: 0.7,
		Company:    "TestCorp",
		Position:   "Go Developer",
	}

	session.AddResult(jd)

	if !jd.IsPassed {
		t.Error("expected jd to be passed")
	}
	if jd.ScoringSessionID != "session-1" {
		t.Errorf("expected ScoringSessionID=session-1, got %s", jd.ScoringSessionID)
	}
	if len(session.Results) != 1 {
		t.Errorf("expected 1 result, got %d", len(session.Results))
	}
}

func TestJDScoringSession_AddResult_RejectedByTrust(t *testing.T) {
	session := &JDScoringSession{
		ID:             "session-1",
		Keywords:       []string{"Go"},
		TrustThreshold: 0.6,
		MatchThreshold: 0.6,
	}

	jd := &ScoredJD{
		ID:         "jd-1",
		TrustScore: 0.3,
		MatchScore: 0.8,
		Company:    "TestCorp",
		Position:   "Go Developer",
	}

	session.AddResult(jd)

	if jd.IsPassed {
		t.Error("expected jd to be rejected")
	}
	if jd.RejectReason == "" {
		t.Error("expected reject reason to be set")
	}
}

func TestJDScoringSession_AddResult_RejectedByMatch(t *testing.T) {
	session := &JDScoringSession{
		ID:             "session-1",
		Keywords:       []string{"Go"},
		TrustThreshold: 0.6,
		MatchThreshold: 0.6,
	}

	jd := &ScoredJD{
		ID:         "jd-1",
		TrustScore: 0.8,
		MatchScore: 0.3,
	}

	session.AddResult(jd)

	if jd.IsPassed {
		t.Error("expected jd to be rejected")
	}
	if jd.RejectReason == "" {
		t.Error("expected reject reason to be set")
	}
}

func TestJDScoringSession_GetPassedResults(t *testing.T) {
	session := &JDScoringSession{
		TrustThreshold: 0.6,
		MatchThreshold: 0.6,
	}

	session.AddResult(&ScoredJD{ID: "jd-1", TrustScore: 0.8, MatchScore: 0.7})
	session.AddResult(&ScoredJD{ID: "jd-2", TrustScore: 0.3, MatchScore: 0.9})
	session.AddResult(&ScoredJD{ID: "jd-3", TrustScore: 0.9, MatchScore: 0.4})

	passed := session.GetPassedResults()
	if len(passed) != 1 {
		t.Errorf("expected 1 passed, got %d", len(passed))
	}
	if passed[0].ID != "jd-1" {
		t.Errorf("expected passed jd-1, got %s", passed[0].ID)
	}
}

func TestJDScoringSession_GetRejectedResults(t *testing.T) {
	session := &JDScoringSession{
		TrustThreshold: 0.6,
		MatchThreshold: 0.6,
	}

	session.AddResult(&ScoredJD{ID: "jd-1", TrustScore: 0.8, MatchScore: 0.7})
	session.AddResult(&ScoredJD{ID: "jd-2", TrustScore: 0.3, MatchScore: 0.9})

	rejected := session.GetRejectedResults()
	if len(rejected) != 1 {
		t.Errorf("expected 1 rejected, got %d", len(rejected))
	}
}

func TestJDScoringSession_MarkCompleted(t *testing.T) {
	session := &JDScoringSession{Status: ScoringPending}
	session.MarkCompleted()

	if session.Status != ScoringCompleted {
		t.Errorf("expected completed, got %s", session.Status)
	}
	if session.CompletedAt == nil {
		t.Error("expected CompletedAt to be set")
	}
}

func TestJDScoringSession_MarkFailed(t *testing.T) {
	session := &JDScoringSession{Status: ScoringPending}
	session.MarkFailed()

	if session.Status != ScoringFailed {
		t.Errorf("expected failed, got %s", session.Status)
	}
}
