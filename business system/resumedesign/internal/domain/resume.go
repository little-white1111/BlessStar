package domain

import "time"

// Resume 简历聚合根
// 不变量：原始简历创建后不可修改（immutable template）
type Resume struct {
	ID           string     `json:"id"`
	Title        string     `json:"title"`
	RawContent   string     `json:"raw_content"`
	Sections     []*Section `json:"sections,omitempty"`
	ImportFormat string     `json:"import_format"` // pdf | docx | txt
	CreatedAt    time.Time  `json:"created_at"`
	UpdatedAt    time.Time  `json:"updated_at"`
}

// AsPrompt 将简历转为 LLM 可消费的提示上下文
func (r *Resume) AsPrompt() string {
	text := "原始简历内容：\n"
	for _, s := range r.Sections {
		text += "\n【" + s.Title + "】\n" + s.Content + "\n"
	}
	return text
}
