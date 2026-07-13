package domain

import "time"

// DeliveryTarget 投递目标实体
// 不变量 D1：投即留痕 — 每次投递操作必须生成持久化记录
// 不变量 D3：文件与记录共存亡
type DeliveryTarget struct {
	ID               string
	SessionID        string
	ScoredJDID       string
	Company          string
	Position         string
	URL              string
	ExportedFormat   string
	ExportedFilePath string
	Status           TargetStatus
	SubmittedAt      time.Time
	ExpireAt         time.Time
}

// NewDeliveryTarget 创建新的投递目标
func NewDeliveryTarget(sessionID, scoredJDID, company, position, url string, expireAt time.Time) *DeliveryTarget {
	return &DeliveryTarget{
		SessionID:  sessionID,
		ScoredJDID: scoredJDID,
		Company:    company,
		Position:   position,
		URL:        url,
		Status:     TargetPending,
		ExpireAt:   expireAt,
	}
}

// MarkExported 标记为已导出
func (t *DeliveryTarget) MarkExported(format, filePath string) {
	t.ExportedFormat = format
	t.ExportedFilePath = filePath
	t.Status = TargetExported
	t.SubmittedAt = time.Now()
}

// MarkCompleted 标记为已完成
func (t *DeliveryTarget) MarkCompleted() {
	t.Status = TargetCompleted
}

// MarkFailed 标记为失败
func (t *DeliveryTarget) MarkFailed() {
	t.Status = TargetFailed
}

// MarkExpired 标记为过期
func (t *DeliveryTarget) MarkExpired() {
	t.Status = TargetExpired
}

// IsExpired 判断是否过期
func (t *DeliveryTarget) IsExpired() bool {
	return time.Now().After(t.ExpireAt)
}
