package parser

import (
	"os"
	"strings"

	"resumedesign/internal/domain"
	"resumedesign/internal/port"
)

// TXTParser 纯文本简历解析器
type TXTParser struct{}

func NewTXTParser() *TXTParser {
	return &TXTParser{}
}

func (p *TXTParser) Parse(filePath string, format string) ([]port.ParsedSection, error) {
	data, err := os.ReadFile(filePath)
	if err != nil {
		return nil, err
	}

	content := string(data)
	lines := strings.Split(content, "\n")

	var sections []port.ParsedSection
	var current port.ParsedSection
	sectionOrder := 0

	for _, line := range lines {
		trimmed := strings.TrimSpace(line)
		if trimmed == "" {
			continue
		}

		// 检测段落标题（以【】、##、===等作为分隔标记）
		sectionType, title := detectSectionType(trimmed)
		if sectionType != "" {
			// 保存上一个段落
			if current.Type != "" {
				current.Content = strings.TrimSpace(current.Content)
				sections = append(sections, current)
			}
			// 开始新段落
			current = port.ParsedSection{
				Type:    sectionType,
				Title:   title,
				Content: "",
				Order:   sectionOrder,
			}
			sectionOrder++
		} else if current.Type != "" {
			if current.Content != "" {
				current.Content += "\n"
			}
			current.Content += trimmed
		}
	}

	// 保存最后一个段落
	if current.Type != "" {
		current.Content = strings.TrimSpace(current.Content)
		sections = append(sections, current)
	}

	return sections, nil
}

// detectSectionType 检测段落类型
func detectSectionType(line string) (domain.SectionType, string) {
	title := strings.Trim(line, "# 【】= \t\r")
	title = strings.TrimSpace(title)

	keywordMap := map[string]domain.SectionType{
		"个人信息":   domain.SectionPersonal,
		"基本资料":   domain.SectionPersonal,
		"联系方式":   domain.SectionPersonal,
		"教育背景":   domain.SectionEducation,
		"教育经历":   domain.SectionEducation,
		"工作经历":   domain.SectionWork,
		"工作经验":   domain.SectionWork,
		"项目经历":   domain.SectionProject,
		"项目经验":   domain.SectionProject,
		"专业技能":   domain.SectionSkill,
		"技能":     domain.SectionSkill,
		"成就奖项":   domain.SectionAward,
		"获奖情况":   domain.SectionAward,
	}

	for kw, st := range keywordMap {
		if strings.Contains(title, kw) {
			return st, title
		}
	}
	return "", ""
}
