package keyword

import (
	"testing"
)

func TestExpand_GoBackend(t *testing.T) {
	dict := DefaultDict()
	expander := NewExpander(dict)

	result, err := expander.Expand([]string{"Go", "后端"})
	if err != nil {
		t.Fatalf("Expand() error = %v", err)
	}

	if len(result.Originals) != 2 {
		t.Errorf("expected 2 originals, got %d: %v", len(result.Originals), result.Originals)
	}

	// Should contain Golang as synonym for Go
	foundGo := false
	for _, s := range result.Synonyms {
		if s == "golang" || s == "Golang" {
			foundGo = true
			break
		}
	}
	if !foundGo {
		t.Errorf("expected 'golang' in synonyms, got %v", result.Synonyms)
	}

	// Should have category expansions for "后端"
	if len(result.Categories) == 0 {
		t.Errorf("expected category expansions for backend, got none")
	}
}

func TestExpand_Deduplication(t *testing.T) {
	dict := DefaultDict()
	expander := NewExpander(dict)

	result, err := expander.Expand([]string{"Go", "golang", "Go"})
	if err != nil {
		t.Fatalf("Expand() error = %v", err)
	}

	if len(result.Originals) != 1 {
		t.Errorf("expected 1 unique original, got %d: %v", len(result.Originals), result.Originals)
	}
}

func TestMatchDetail_PartialMatch(t *testing.T) {
	dict := DefaultDict()
	expander := NewExpander(dict)

	expanded, _ := expander.Expand([]string{"Go", "后端"})
	detail := expander.MatchDetail("Go Developer", "精通 Go 语言开发，熟悉微服务架构", expanded)

	if detail.TotalScore <= 0 {
		t.Errorf("expected positive match score, got %f", detail.TotalScore)
	}
	if len(detail.TitleMatches) == 0 {
		t.Error("expected title matches for 'Go'")
	}
	if len(detail.ContentMatches) == 0 {
		t.Error("expected content matches")
	}
}

func TestMatchDetail_NoMatch(t *testing.T) {
	dict := DefaultDict()
	expander := NewExpander(dict)

	expanded, _ := expander.Expand([]string{"Rust"})
	detail := expander.MatchDetail("Python Developer", "数据分析和机器学习", expanded)

	if detail.TotalScore > 0.1 {
		t.Errorf("expected near-zero match score for unrelated content, got %f", detail.TotalScore)
	}
}

func TestMatchScore_ExactMatch(t *testing.T) {
	dict := DefaultDict()
	expander := NewExpander(dict)

	expanded, _ := expander.Expand([]string{"Go"})
	score := expander.MatchScore("Go Developer", "精通 Go 语言开发", expanded)

	if score <= 0 {
		t.Errorf("expected positive score, got %f", score)
	}
}

func TestDict_GetSynonyms(t *testing.T) {
	dict := DefaultDict()

	syns := dict.GetSynonyms("go")
	if len(syns) == 0 {
		t.Error("expected synonyms for 'go'")
	}

	syns = dict.GetSynonyms("nonexistent")
	if syns != nil {
		t.Errorf("expected nil for nonexistent, got %v", syns)
	}
}

func TestDict_AllCategories(t *testing.T) {
	dict := DefaultDict()
	cats := dict.AllCategories()

	if len(cats) == 0 {
		t.Error("expected non-empty categories")
	}

	found := false
	for _, c := range cats {
		if c == "后端" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected '后端' in categories, got %v", cats)
	}
}
