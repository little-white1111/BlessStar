package domain

import (
	"strings"
	"testing"
)

func TestJobDescriptionAsPrompt(t *testing.T) {
	jd := &JobDescription{
		ID:         "test-id",
		Company:    "某大厂",
		Position:   "后端开发",
		RawContent: "精通 Go、熟悉分布式系统",
	}

	prompt := jd.AsPrompt()

	if !strings.Contains(prompt, "岗位要求") {
		t.Error("AsPrompt should contain job description header")
	}
	if !strings.Contains(prompt, "某大厂") {
		t.Error("AsPrompt should contain company name")
	}
	if !strings.Contains(prompt, "后端开发") {
		t.Error("AsPrompt should contain position")
	}
	if !strings.Contains(prompt, "精通 Go") {
		t.Error("AsPrompt should contain raw content")
	}
}

func TestJobDescriptionExtractKeywords(t *testing.T) {
	jd := &JobDescription{
		RawContent: "精通 Go、熟悉 Kubernetes、了解微服务架构",
	}

	keywords := jd.ExtractKeywords()

	expectedKeywords := []string{"精通 Go", "熟悉 Kubernetes", "了解微服务架构"}
	for _, ek := range expectedKeywords {
		found := false
		for _, k := range keywords {
			if k == ek {
				found = true
				break
			}
		}
		if !found {
			t.Errorf("Expected keyword %q not found in %v", ek, keywords)
		}
	}
}

func TestJobDescriptionExtractKeywordsEmpty(t *testing.T) {
	jd := &JobDescription{
		RawContent: "",
	}

	keywords := jd.ExtractKeywords()
	if len(keywords) != 0 {
		t.Errorf("Expected empty keywords, got %v", keywords)
	}
}
