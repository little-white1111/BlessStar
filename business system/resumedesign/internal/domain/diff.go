package domain

// ChangeType 差异变更类型
type ChangeType string

const (
	ChangeModified  ChangeType = "modified"   // 修改
	ChangeAdded     ChangeType = "added"      // 新增
	ChangeRemoved   ChangeType = "removed"    // 删除
	ChangeUnchanged ChangeType = "unchanged"  // 未变更
)

// Diff 简历差异值对象
// 记录改写前后对应段落的变更详情
type Diff struct {
	ID                string     `json:"id"`
	SessionID         string     `json:"session_id"`
	SectionType       SectionType `json:"section_type"`
	OriginalContent   string     `json:"original_content"`
	RewrittenContent  string     `json:"rewritten_content"`
	ChangeType        ChangeType `json:"change_type"`
	Confidence        float64    `json:"confidence"`
	SortOrder         int        `json:"sort_order"`
}

// IsValidChangeType 判断变更类型是否合法
func IsValidChangeType(c ChangeType) bool {
	switch c {
	case ChangeModified, ChangeAdded, ChangeRemoved, ChangeUnchanged:
		return true
	}
	return false
}
