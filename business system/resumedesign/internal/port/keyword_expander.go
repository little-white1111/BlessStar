package port

// ExpandedKeywords 扩展后的关键词集合
type ExpandedKeywords struct {
	Originals  []string
	Synonyms   []string
	Categories []string
}

// MatchDetail 关键词匹配明细
type MatchDetail struct {
	TitleMatches    []string
	ContentMatches  []string
	SynonymMatches  []string
	CategoryMatches []string
	TotalScore      float64
}

// KeywordExpander 关键词扩展与匹配 Port 接口
// 职责：扩展用户输入的关键词为同义词+类别集合，并计算匹配度
// 不变量 S5：同义词映射表和类别映射表应可动态配置
type KeywordExpander interface {
	// Expand 扩展关键词为同义词+类别集合
	Expand(keywords []string) (*ExpandedKeywords, error)
	// MatchScore 计算 JD 标题+内容与扩展关键词的匹配度（0~1）
	MatchScore(title string, content string, expanded *ExpandedKeywords) float64
	// MatchDetail 返回匹配明细（匹配了哪些关键词/同义词/类别）
	MatchDetail(title string, content string, expanded *ExpandedKeywords) *MatchDetail
}
