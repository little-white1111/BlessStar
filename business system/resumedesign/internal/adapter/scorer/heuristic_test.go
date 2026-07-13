package scorer

import (
	"testing"

	"resumedesign/internal/port"
)

func TestDetectSalaryRange_WithSalary(t *testing.T) {
	content := "薪资范围：15k-25k，根据能力面议"
	if !detectSalaryRange(content) {
		t.Error("expected salary range detected")
	}
}

func TestDetectSalaryRange_WithKRange(t *testing.T) {
	content := "月薪 15K-30K"
	if !detectSalaryRange(content) {
		t.Error("expected K-range salary detected")
	}
}

func TestDetectSalaryRange_NoSalary(t *testing.T) {
	content := "这是一个很好的公司，氛围好，成长快"
	if detectSalaryRange(content) {
		t.Error("expected no salary range detected")
	}
}

func TestDetectBenefits_WithBenefits(t *testing.T) {
	content := "五险一金、带薪年假、年终奖"
	if !detectBenefits(content) {
		t.Error("expected benefits detected")
	}
}

func TestDetectBenefits_NoBenefits(t *testing.T) {
	content := "技术要求：精通 Go 语言"
	if detectBenefits(content) {
		t.Error("expected no benefits detected")
	}
}

func TestDetectCompanyIntro_WithIntro(t *testing.T) {
	content := "公司简介：我们是一家专注于 AI 技术的公司"
	if !detectCompanyIntro(content) {
		t.Error("expected company intro detected")
	}
}

func TestDetectCompanyIntro_NoIntro(t *testing.T) {
	content := "岗位职责：负责后端开发"
	if detectCompanyIntro(content) {
		t.Error("expected no company intro detected")
	}
}

func TestDetectLocationMatch_Same(t *testing.T) {
	content := "工作地点：北京海淀区"
	result := detectLocationMatch(content, "北京")
	if result != "same" {
		t.Errorf("expected 'same', got '%s'", result)
	}
}

func TestDetectLocationMatch_Near(t *testing.T) {
	content := "办公地址在上海市浦东新区"
	result := detectLocationMatch(content, "上海")
	if result != "near" && result != "same" {
		t.Errorf("expected 'near' or 'same', got '%s'", result)
	}
}

func TestDetectLocationMatch_Different(t *testing.T) {
	content := "工作地点：深圳南山区"
	result := detectLocationMatch(content, "北京")
	if result != "near" && result != "different" {
		t.Errorf("expected 'near' or 'different', got '%s'", result)
	}
}

func TestHeuristicScorer_FullScore(t *testing.T) {
	scorer := NewHeuristicScorer("北京")
	content := `
		薪资范围：20k-40k
		五险一金、年终奖、股票期权
		公司简介：成立于2015年，专注于AI技术
		工作地点：北京海淀区
	`
	jd := &port.FetchedJD{
		RawContent: content,
	}

	score, err := scorer.Score(jd, "北京")
	if err != nil {
		t.Fatalf("Score() error = %v", err)
	}

	if score.Details.HasSalaryRange != true {
		t.Error("expected HasSalaryRange=true")
	}
	if score.Details.HasBenefits != true {
		t.Error("expected HasBenefits=true")
	}
	if score.Details.HasCompanyIntro != true {
		t.Error("expected HasCompanyIntro=true")
	}
	if score.Score <= 0 {
		t.Errorf("expected positive score, got %f", score.Score)
	}
}

func TestHeuristicScorer_EmptyContent(t *testing.T) {
	scorer := NewHeuristicScorer("北京")
	jd := &port.FetchedJD{RawContent: ""}

	score, err := scorer.Score(jd, "北京")
	if err != nil {
		t.Fatalf("Score() error = %v", err)
	}

	if score.Score != 0 {
		t.Errorf("expected 0 for empty content, got %f", score.Score)
	}
}
