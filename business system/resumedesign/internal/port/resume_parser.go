package port

import "resumedesign/internal/domain"

// ResumeParser 简历文件解析器 Port 接口
// 职责：将原始文件解析为结构化段落
// 实现：adapter/parser/pdf.go（pdfcpu），adapter/parser/docx.go（自建）
type ResumeParser interface {
	// Parse 解析简历文件
	// filePath: 已保存到临时目录的文件路径
	// format: "pdf" | "docx" | "txt"
	// 返回: 段落列表 + 错误
	Parse(filePath string, format string) ([]ParsedSection, error)
}

// ParsedSection 解析后的段落
type ParsedSection struct {
	Type    domain.SectionType
	Title   string
	Content string
	Order   int
}
