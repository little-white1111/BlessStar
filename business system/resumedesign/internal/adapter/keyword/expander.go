package keyword

import (
	"strings"

	"resumedesign/internal/port"
)

// Expander 关键词扩展与匹配实现
type Expander struct {
	dict *Dict
}

func NewExpander(dict *Dict) *Expander {
	return &Expander{dict: dict}
}

// Expand 扩展关键词为同义词+类别集合
func (e *Expander) Expand(keywords []string) (*port.ExpandedKeywords, error) {
	result := &port.ExpandedKeywords{
		Originals:  make([]string, 0),
		Synonyms:   make([]string, 0),
		Categories: make([]string, 0),
	}

	seen := make(map[string]bool)

	for _, kw := range keywords {
		kw = strings.TrimSpace(kw)
		if kw == "" || seen[toLower(kw)] {
			continue
		}
		seen[toLower(kw)] = true
		result.Originals = append(result.Originals, kw)

		// 扩展同义词
		syns := e.dict.GetSynonyms(kw)
		for _, syn := range syns {
			lowerSyn := toLower(syn)
			if !seen[lowerSyn] {
				seen[lowerSyn] = true
				result.Synonyms = append(result.Synonyms, syn)
			}
		}

		// 扩展类别
		for _, cat := range e.dict.AllCategories() {
			if toLower(cat) == toLower(kw) || contains(toLower(cat), toLower(kw)) {
				catKws := e.dict.GetCategoryKeywords(cat)
				for _, ckw := range catKws {
					lowerCkw := toLower(ckw)
					if !seen[lowerCkw] {
						seen[lowerCkw] = true
						result.Categories = append(result.Categories, ckw)
					}
				}
				// 将类别名本身加入 categories
				if !seen[toLower(cat)] {
					seen[toLower(cat)] = true
					result.Categories = append(result.Categories, cat)
				}
			}
		}
	}

	return result, nil
}

// MatchScore 计算匹配度（0~1）
func (e *Expander) MatchScore(title string, content string, expanded *port.ExpandedKeywords) float64 {
	detail := e.MatchDetail(title, content, expanded)
	return detail.TotalScore
}

// MatchDetail 返回匹配明细
func (e *Expander) MatchDetail(title string, content string, expanded *port.ExpandedKeywords) *port.MatchDetail {
	detail := &port.MatchDetail{}

	titleLower := toLower(title)
	contentLower := toLower(content)
	combinedLower := titleLower + "\n" + contentLower

	// 匹配原始关键词
	for _, kw := range expanded.Originals {
		kwLower := toLower(kw)
		if strings.Contains(titleLower, kwLower) {
			detail.TitleMatches = append(detail.TitleMatches, kw)
		}
		if strings.Contains(contentLower, kwLower) {
			detail.ContentMatches = append(detail.ContentMatches, kw)
		}
	}

	// 匹配同义词
	for _, syn := range expanded.Synonyms {
		synLower := toLower(syn)
		if strings.Contains(combinedLower, synLower) {
			detail.SynonymMatches = append(detail.SynonymMatches, syn)
		}
	}

	// 匹配类别关键词
	for _, cat := range expanded.Categories {
		catLower := toLower(cat)
		if strings.Contains(combinedLower, catLower) {
			detail.CategoryMatches = append(detail.CategoryMatches, cat)
		}
	}

	// 计算总分（基于唯一匹配数）
	totalKeywords := len(expanded.Originals) + len(expanded.Synonyms) + len(expanded.Categories)
	if totalKeywords == 0 {
		detail.TotalScore = 0
		return detail
	}

	matched := len(detail.TitleMatches) + len(detail.ContentMatches) +
		len(detail.SynonymMatches) + len(detail.CategoryMatches)

	if matched > totalKeywords {
		matched = totalKeywords
	}
	detail.TotalScore = float64(matched) / float64(totalKeywords)
	return detail
}

func contains(s, substr string) bool {
	return strings.Contains(s, substr)
}
