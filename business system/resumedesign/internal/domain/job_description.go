package domain

import (
	"strings"
	"time"
)

// JobDescription 岗位要求实体
type JobDescription struct {
	ID          string    `json:"id"`
	Source      string    `json:"source"`       // url | api | manual
	SourceURL   string    `json:"source_url"`   // 来源 URL（可选）
	Company     string    `json:"company"`       // 公司名称
	Position    string    `json:"position"`      // 职位名称
	RawContent  string    `json:"raw_content"`   // JD 原始全文
	CreatedAt   time.Time `json:"created_at"`
}

// ExtractKeywords 提取岗位要求关键词
func (jd *JobDescription) ExtractKeywords() []string {
	// 简单实现：按常见分隔符拆分并去重
	parts := strings.FieldsFunc(jd.RawContent, func(r rune) bool {
		return r == '，' || r == '、' || r == '。' || r == ',' || r == ';' || r == '；' || r == '\n'
	})
	seen := make(map[string]bool, len(parts))
	keywords := make([]string, 0, len(parts))
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if p != "" && !seen[p] {
			seen[p] = true
			keywords = append(keywords, p)
		}
	}
	return keywords
}

// AsPrompt 将 JD 转为 LLM 可消费的提示上下文
func (jd *JobDescription) AsPrompt() string {
	return "岗位要求（JD）：\n公司：" + jd.Company + "\n职位：" + jd.Position + "\n要求：\n" + jd.RawContent
}
