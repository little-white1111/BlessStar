package domain

import (
	"testing"
	"time"
)

func TestNewDeliveryTarget(t *testing.T) {
	expireAt := time.Now().Add(7 * 24 * time.Hour)
	target := NewDeliveryTarget("session-1", "jd-1", "Corp", "Dev", "https://example.com", expireAt)

	if target.SessionID != "session-1" {
		t.Errorf("expected session-1, got %s", target.SessionID)
	}
	if target.ScoredJDID != "jd-1" {
		t.Errorf("expected jd-1, got %s", target.ScoredJDID)
	}
	if target.Company != "Corp" {
		t.Errorf("expected Corp, got %s", target.Company)
	}
	if target.Position != "Dev" {
		t.Errorf("expected Dev, got %s", target.Position)
	}
	if target.URL != "https://example.com" {
		t.Errorf("expected example.com, got %s", target.URL)
	}
	if target.Status != TargetPending {
		t.Errorf("expected pending, got %s", target.Status)
	}
}

func TestDeliveryTarget_MarkExported(t *testing.T) {
	target := NewDeliveryTarget("s1", "jd-1", "Corp", "Dev", "url", time.Now().Add(7*24*time.Hour))
	target.MarkExported("pdf", "/path/to/file.pdf")

	if target.Status != TargetExported {
		t.Errorf("expected exported, got %s", target.Status)
	}
	if target.ExportedFormat != "pdf" {
		t.Errorf("expected pdf, got %s", target.ExportedFormat)
	}
	if target.ExportedFilePath != "/path/to/file.pdf" {
		t.Errorf("expected /path/to/file.pdf, got %s", target.ExportedFilePath)
	}
	if target.SubmittedAt.IsZero() {
		t.Error("expected SubmittedAt to be set")
	}
}

func TestDeliveryTarget_StatusTransitions(t *testing.T) {
	target := NewDeliveryTarget("s1", "jd-1", "Corp", "Dev", "url", time.Now().Add(7*24*time.Hour))

	target.MarkExported("pdf", "file.pdf")
	if target.Status != TargetExported {
		t.Errorf("expected exported, got %s", target.Status)
	}

	target.MarkCompleted()
	if target.Status != TargetCompleted {
		t.Errorf("expected completed, got %s", target.Status)
	}

	target = NewDeliveryTarget("s1", "jd-1", "Corp", "Dev", "url", time.Now().Add(7*24*time.Hour))
	target.MarkFailed()
	if target.Status != TargetFailed {
		t.Errorf("expected failed, got %s", target.Status)
	}

	target.MarkExpired()
	if target.Status != TargetExpired {
		t.Errorf("expected expired, got %s", target.Status)
	}
}

func TestDeliveryTarget_IsExpired(t *testing.T) {
	target := NewDeliveryTarget("s1", "jd-1", "Corp", "Dev", "url", time.Now().Add(7*24*time.Hour))
	if target.IsExpired() {
		t.Error("new target should not be expired")
	}

	target.ExpireAt = time.Now().Add(-1 * time.Hour)
	if !target.IsExpired() {
		t.Error("target with past expire time should be expired")
	}
}
