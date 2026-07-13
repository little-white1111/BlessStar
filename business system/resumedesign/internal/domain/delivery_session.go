package domain

import "time"

// DeliverySession 批量投递会话聚合根
// 不变量 D2：过期必删
// 不变量 D4：单份简历单次投递
// 不变量 D5：TTL 创建锁定，不允许修改
type DeliverySession struct {
	ID          string
	ResumeID    string
	ResumeTitle string
	TTL         time.Duration
	ExpireAt    time.Time
	Status      DeliveryStatus
	Targets     []*DeliveryTarget
	CreatedAt   time.Time
}

// NewDeliverySession 创建新的投递会话
// D5：TTL 在创建时锁定
func NewDeliverySession(resumeID, resumeTitle string, ttl time.Duration) *DeliverySession {
	now := time.Now()
	return &DeliverySession{
		ID:          "",
		ResumeID:    resumeID,
		ResumeTitle: resumeTitle,
		TTL:         ttl,
		ExpireAt:    now.Add(ttl),
		Status:      DeliveryPending,
		Targets:     make([]*DeliveryTarget, 0),
		CreatedAt:   now,
	}
}

// AddTarget 添加投递目标
func (s *DeliverySession) AddTarget(target *DeliveryTarget) {
	target.SessionID = s.ID
	target.ExpireAt = s.ExpireAt
	s.Targets = append(s.Targets, target)
}

// SetCompleted 标记完成
func (s *DeliverySession) SetCompleted() {
	s.Status = DeliveryCompleted
	for _, t := range s.Targets {
		if t.Status == TargetExported {
			t.Status = TargetCompleted
		}
	}
}

// IsExpired 判断是否过期
func (s *DeliverySession) IsExpired() bool {
	return time.Now().After(s.ExpireAt)
}

// RemainingDays 剩余天数
func (s *DeliverySession) RemainingDays() int {
	remaining := time.Until(s.ExpireAt)
	days := int(remaining.Hours() / 24)
	if days < 0 {
		return 0
	}
	return days
}
