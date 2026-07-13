package domain

import "time"

// SessionStatus 改写会话状态
type SessionStatus string

const (
	SessionDraft      SessionStatus = "draft"      // 草稿（已创建未改写）
	SessionProcessing SessionStatus = "processing" // 改写中
	SessionCompleted  SessionStatus = "completed"  // 改写完成
	SessionFailed     SessionStatus = "failed"     // 改写失败
)

// RewriteConfig 改写配置
type RewriteConfig struct {
	Model       string  `json:"model"`
	Temperature float64 `json:"temperature"`
	MaxLength   int     `json:"max_length"`
}

// RewriteSession 改写会话聚合根
// 不变量：每次状态变更必须持久化到数据库（T4）
type RewriteSession struct {
	ID               string         `json:"id"`
	ResumeID         string         `json:"resume_id"`
	JdID             string         `json:"jd_id"`
	Status           SessionStatus  `json:"status"`
	Model            string         `json:"model"`
	Temperature      float64        `json:"temperature"`
	RewrittenContent string         `json:"rewritten_content"`
	PromptHash       string         `json:"prompt_hash"`
	ErrorMessage     string         `json:"error_message,omitempty"`
	Diff             []*Diff        `json:"diffs,omitempty"`
	CreatedAt        time.Time      `json:"created_at"`
	CompletedAt      *time.Time     `json:"completed_at,omitempty"`
}

// IsValidSessionStatus 判断会话状态是否合法
func IsValidSessionStatus(s SessionStatus) bool {
	switch s {
	case SessionDraft, SessionProcessing, SessionCompleted, SessionFailed:
		return true
	}
	return false
}
