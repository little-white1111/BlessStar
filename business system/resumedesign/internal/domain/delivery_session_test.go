package domain

import (
	"testing"
	"time"
)

func TestNewDeliverySession(t *testing.T) {
	ttl := 7 * 24 * time.Hour
	session := NewDeliverySession("resume-1", "我的简历", ttl)

	if session.ResumeID != "resume-1" {
		t.Errorf("expected resume-1, got %s", session.ResumeID)
	}
	if session.ResumeTitle != "我的简历" {
		t.Errorf("expected 我的简历, got %s", session.ResumeTitle)
	}
	if session.TTL != ttl {
		t.Errorf("expected %v, got %v", ttl, session.TTL)
	}
	if session.Status != DeliveryPending {
		t.Errorf("expected pending, got %s", session.Status)
	}
	if session.ExpireAt.IsZero() {
		t.Error("expected ExpireAt to be set")
	}
	if !session.ExpireAt.After(time.Now()) {
		t.Error("expected ExpireAt to be in the future")
	}
}

func TestDeliverySession_AddTarget(t *testing.T) {
	session := NewDeliverySession("resume-1", "我的简历", 7*24*time.Hour)
	session.ID = "session-1"

	target := NewDeliveryTarget("", "jd-1", "Corp", "Dev", "https://example.com", time.Now().Add(7*24*time.Hour))
	session.AddTarget(target)

	if len(session.Targets) != 1 {
		t.Errorf("expected 1 target, got %d", len(session.Targets))
	}
	if target.SessionID != "session-1" {
		t.Errorf("expected session-1, got %s", target.SessionID)
	}
}

func TestDeliverySession_SetCompleted(t *testing.T) {
	session := NewDeliverySession("resume-1", "我的简历", 7*24*time.Hour)
	session.AddTarget(&DeliveryTarget{Status: TargetExported})
	session.SetCompleted()

	if session.Status != DeliveryCompleted {
		t.Errorf("expected completed, got %s", session.Status)
	}
	if session.Targets[0].Status != TargetCompleted {
		t.Errorf("expected target completed, got %s", session.Targets[0].Status)
	}
}

func TestDeliverySession_IsExpired(t *testing.T) {
	session := NewDeliverySession("resume-1", "我的简历", 7*24*time.Hour)
	if session.IsExpired() {
		t.Error("new session should not be expired")
	}

	// Test with past expire time
	session.ExpireAt = time.Now().Add(-1 * time.Hour)
	if !session.IsExpired() {
		t.Error("session with past expire time should be expired")
	}
}

func TestDeliverySession_RemainingDays(t *testing.T) {
	session := NewDeliverySession("resume-1", "我的简历", 7*24*time.Hour)
	days := session.RemainingDays()
	if days != 7 {
		t.Errorf("expected 7 days remaining, got %d", days)
	}

	// Test expired
	session.ExpireAt = time.Now().Add(-1 * time.Hour)
	days = session.RemainingDays()
	if days != 0 {
		t.Errorf("expected 0 days for expired, got %d", days)
	}
}

func TestDeliverySession_TTLLocked(t *testing.T) {
	// D5 invariant: TTL locked at creation
	ttl := 7 * 24 * time.Hour
	session := NewDeliverySession("resume-1", "我的简历", ttl)

	if session.TTL != ttl {
		t.Errorf("expected TTL %v to be locked, got %v", ttl, session.TTL)
	}
}
