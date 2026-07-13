package domain

// SectionType 简历段落类型
type SectionType string

const (
	SectionPersonal    SectionType = "personal"    // 个人信息
	SectionEducation   SectionType = "education"   // 教育背景
	SectionWork        SectionType = "work"        // 工作经历
	SectionProject     SectionType = "project"     // 项目经历
	SectionSkill       SectionType = "skill"       // 专业技能
	SectionAward       SectionType = "award"       // 成就奖项
)

// Section 简历段落值对象
type Section struct {
	ID      string      `json:"id"`
	ResumeID string     `json:"resume_id"`
	Type    SectionType `json:"type"`
	Title   string      `json:"title"`
	Content string      `json:"content"`
	Order   int         `json:"order"`
}

// ValidSectionTypes 返回所有合法的段落类型
func ValidSectionTypes() []SectionType {
	return []SectionType{
		SectionPersonal,
		SectionEducation,
		SectionWork,
		SectionProject,
		SectionSkill,
		SectionAward,
	}
}

// IsValidSectionType 判断段落类型是否合法
func IsValidSectionType(t SectionType) bool {
	for _, vt := range ValidSectionTypes() {
		if vt == t {
			return true
		}
	}
	return false
}
