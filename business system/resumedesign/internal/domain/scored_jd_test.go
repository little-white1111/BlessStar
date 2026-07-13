package domain

import "testing"

func TestIsValidScoringStatus(t *testing.T) {
	tests := []struct {
		name  string
		ss    ScoringStatus
		valid bool
	}{
		{"pending", ScoringPending, true},
		{"completed", ScoringCompleted, true},
		{"failed", ScoringFailed, true},
		{"invalid", "unknown", false},
		{"empty", ScoringStatus(""), false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidScoringStatus(tt.ss); got != tt.valid {
				t.Errorf("IsValidScoringStatus(%q) = %v, want %v", tt.ss, got, tt.valid)
			}
		})
	}
}

func TestIsValidTargetStatus(t *testing.T) {
	tests := []struct {
		name  string
		ts    TargetStatus
		valid bool
	}{
		{"pending", TargetPending, true},
		{"exported", TargetExported, true},
		{"completed", TargetCompleted, true},
		{"failed", TargetFailed, true},
		{"expired", TargetExpired, true},
		{"invalid", "unknown", false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidTargetStatus(tt.ts); got != tt.valid {
				t.Errorf("IsValidTargetStatus(%q) = %v, want %v", tt.ts, got, tt.valid)
			}
		})
	}
}

func TestIsValidDeliveryStatus(t *testing.T) {
	tests := []struct {
		name  string
		ds    DeliveryStatus
		valid bool
	}{
		{"pending", DeliveryPending, true},
		{"completed", DeliveryCompleted, true},
		{"expired", DeliveryExpired, true},
		{"invalid", "unknown", false},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidDeliveryStatus(tt.ds); got != tt.valid {
				t.Errorf("IsValidDeliveryStatus(%q) = %v, want %v", tt.ds, got, tt.valid)
			}
		})
	}
}

func TestCalculateTrust_FullScore(t *testing.T) {
	jd := &ScoredJD{
		Details: ScoreDetails{
			HasSalaryRange:  true,
			HasBenefits:     true,
			HasCompanyIntro: true,
			LocationMatch:   "same",
			CompanyVerified: true,
		},
	}
	score := jd.CalculateTrust(0.1, 0.2, 0.1, 0.1, 0.1, 0.2)
	expected := 0.7 // 0.1+0.1+0.1+0.2+0.2
	if score != expected {
		t.Errorf("CalculateTrust() = %v, want %v", score, expected)
	}
}

func TestCalculateTrust_PartialScore(t *testing.T) {
	jd := &ScoredJD{
		Details: ScoreDetails{
			HasSalaryRange:  true,
			HasBenefits:     false,
			HasCompanyIntro: false,
			LocationMatch:   "near",
			CompanyVerified: false,
		},
	}
	score := jd.CalculateTrust(0.1, 0.2, 0.1, 0.1, 0.1, 0.2)
	expected := 0.2 // 0.1(salary) + 0.1(location near)
	if score != expected {
		t.Errorf("CalculateTrust() = %v, want %v", score, expected)
	}
}

func TestCalculateTrust_MinScore(t *testing.T) {
	jd := &ScoredJD{
		Details: ScoreDetails{
			HasSalaryRange:  false,
			HasBenefits:     false,
			HasCompanyIntro: false,
			LocationMatch:   "different",
			CompanyVerified: false,
		},
	}
	score := jd.CalculateTrust(0.1, 0.2, 0.1, 0.1, 0.1, 0.2)
	if score != 0 {
		t.Errorf("CalculateTrust() = %v, want 0", score)
	}
}

func TestCalculateTrust_ClampToOne(t *testing.T) {
	jd := &ScoredJD{
		Details: ScoreDetails{
			HasSalaryRange:  true,
			HasBenefits:     true,
			HasCompanyIntro: true,
			LocationMatch:   "same",
			CompanyVerified: true,
		},
	}
	// 使用超大权重测试 clamp
	score := jd.CalculateTrust(0.5, 1.0, 0.5, 0.5, 0.5, 1.0)
	if score > 1.0 {
		t.Errorf("CalculateTrust() = %v, want <= 1.0", score)
	}
}

func TestCalculateMatch_ExactMatch(t *testing.T) {
	jd := &ScoredJD{
		Details: ScoreDetails{
			MatchedKeywords:   []string{"Go", "后端"},
			MatchedSynonyms:   []string{"Golang"},
			MatchedCategories: []string{"Java"},
		},
	}
	score := jd.CalculateMatch(3)
	expected := 1.0 // 3/3
	if score != expected {
		t.Errorf("CalculateMatch() = %v, want %v", score, expected)
	}
}

func TestCalculateMatch_PartialMatch(t *testing.T) {
	jd := &ScoredJD{
		Details: ScoreDetails{
			MatchedKeywords:   []string{"Go"},
			MatchedSynonyms:   []string{},
			MatchedCategories: []string{},
		},
	}
	score := jd.CalculateMatch(5)
	expected := 0.2 // 1/5
	if score != expected {
		t.Errorf("CalculateMatch() = %v, want %v", score, expected)
	}
}

func TestCalculateMatch_NoKeywords(t *testing.T) {
	jd := &ScoredJD{
		Details: ScoreDetails{
			MatchedKeywords:   []string{"Go"},
			MatchedSynonyms:   []string{},
			MatchedCategories: []string{},
		},
	}
	score := jd.CalculateMatch(0)
	if score != 0 {
		t.Errorf("CalculateMatch() with 0 keywords = %v, want 0", score)
	}
}
