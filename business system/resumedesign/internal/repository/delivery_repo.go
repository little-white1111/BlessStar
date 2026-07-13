package repository

import (
	"time"

	"resumedesign/internal/domain"

	"gorm.io/gorm"
)

// DeliverySessionModel GORM 模型 — delivery_sessions 表
type DeliverySessionModel struct {
	ID          string    `gorm:"primaryKey;size:36"`
	ResumeID    string    `gorm:"not null;size:36"`
	ResumeTitle string    `gorm:"not null;default:''"`
	TTL         int64     `gorm:"not null;default:604800"` // 7 天（秒）
	ExpireAt    time.Time `gorm:"not null;index"`
	Status      string    `gorm:"not null;default:pending;size:20"`
	CreatedAt   time.Time `gorm:"not null"`
}

func (DeliverySessionModel) TableName() string { return "delivery_sessions" }

// DeliveryTargetModel GORM 模型 — delivery_targets 表
type DeliveryTargetModel struct {
	ID              string    `gorm:"primaryKey;size:36"`
	SessionID       string    `gorm:"not null;index;size:36"`
	ScoredJDID      string    `gorm:"not null;default:'';size:36"`
	Company         string    `gorm:"not null;default:''"`
	Position        string    `gorm:"not null;default:''"`
	URL             string    `gorm:"not null;default:''"`
	ExportedFormat  string    `gorm:"not null;default:'';size:10"`
	ExportedFilePath string   `gorm:"not null;default:''"`
	Status          string    `gorm:"not null;default:pending;size:20"`
	SubmittedAt     time.Time `gorm:"default:null"`
	ExpireAt        time.Time `gorm:"not null;index"`
}

func (DeliveryTargetModel) TableName() string { return "delivery_targets" }

// DeliveryRepo 批量投递数据仓储
type DeliveryRepo struct {
	db *gorm.DB
}

func NewDeliveryRepo(db *gorm.DB) *DeliveryRepo {
	return &DeliveryRepo{db: db}
}

// toDeliverySessionModel Domain → GORM
func toDeliverySessionModel(s *domain.DeliverySession) *DeliverySessionModel {
	return &DeliverySessionModel{
		ID:          s.ID,
		ResumeID:    s.ResumeID,
		ResumeTitle: s.ResumeTitle,
		TTL:         int64(s.TTL.Seconds()),
		ExpireAt:    s.ExpireAt,
		Status:      string(s.Status),
		CreatedAt:   s.CreatedAt,
	}
}

// toDomain GORM → Domain
func (m *DeliverySessionModel) toDomain() *domain.DeliverySession {
	return &domain.DeliverySession{
		ID:          m.ID,
		ResumeID:    m.ResumeID,
		ResumeTitle: m.ResumeTitle,
		TTL:         time.Duration(m.TTL) * time.Second,
		ExpireAt:    m.ExpireAt,
		Status:      domain.DeliveryStatus(m.Status),
		CreatedAt:   m.CreatedAt,
		Targets:     make([]*domain.DeliveryTarget, 0),
	}
}

// toDeliveryTargetModel Domain → GORM
func toDeliveryTargetModel(t *domain.DeliveryTarget) *DeliveryTargetModel {
	return &DeliveryTargetModel{
		ID:               t.ID,
		SessionID:        t.SessionID,
		ScoredJDID:       t.ScoredJDID,
		Company:          t.Company,
		Position:         t.Position,
		URL:              t.URL,
		ExportedFormat:   t.ExportedFormat,
		ExportedFilePath: t.ExportedFilePath,
		Status:           string(t.Status),
		SubmittedAt:      t.SubmittedAt,
		ExpireAt:         t.ExpireAt,
	}
}

// toDomain GORM → Domain
func (m *DeliveryTargetModel) toDomain() *domain.DeliveryTarget {
	return &domain.DeliveryTarget{
		ID:               m.ID,
		SessionID:        m.SessionID,
		ScoredJDID:       m.ScoredJDID,
		Company:          m.Company,
		Position:         m.Position,
		URL:              m.URL,
		ExportedFormat:   m.ExportedFormat,
		ExportedFilePath: m.ExportedFilePath,
		Status:           domain.TargetStatus(m.Status),
		SubmittedAt:      m.SubmittedAt,
		ExpireAt:         m.ExpireAt,
	}
}

// CreateSession 创建投递会话
func (r *DeliveryRepo) CreateSession(s *domain.DeliverySession) error {
	return r.db.Create(toDeliverySessionModel(s)).Error
}

// FindSessionByID 查找投递会话（含目标）
func (r *DeliveryRepo) FindSessionByID(id string) (*domain.DeliverySession, error) {
	var m DeliverySessionModel
	if err := r.db.First(&m, "id = ?", id).Error; err != nil {
		return nil, err
	}
	session := m.toDomain()

	var targetModels []DeliveryTargetModel
	r.db.Where("session_id = ?", id).Find(&targetModels)
	for i := range targetModels {
		session.Targets = append(session.Targets, targetModels[i].toDomain())
	}
	return session, nil
}

// FindAllSessions 获取所有投递会话
func (r *DeliveryRepo) FindAllSessions() ([]*domain.DeliverySession, error) {
	var models []DeliverySessionModel
	if err := r.db.Order("created_at DESC").Find(&models).Error; err != nil {
		return nil, err
	}
	sessions := make([]*domain.DeliverySession, len(models))
	for i, m := range models {
		sessions[i] = m.toDomain()
	}
	return sessions, nil
}

// UpdateSession 更新投递会话
func (r *DeliveryRepo) UpdateSession(s *domain.DeliverySession) error {
	return r.db.Save(toDeliverySessionModel(s)).Error
}

// DeleteSession 删除投递会话（含目标，CASCADE）
func (r *DeliveryRepo) DeleteSession(id string) error {
	r.db.Where("session_id = ?", id).Delete(&DeliveryTargetModel{})
	return r.db.Delete(&DeliverySessionModel{}, "id = ?", id).Error
}

// FindExpiredSessions 获取过期投递会话
func (r *DeliveryRepo) FindExpiredSessions() ([]*domain.DeliverySession, error) {
	var models []DeliverySessionModel
	now := time.Now()
	if err := r.db.Where("expire_at < ?", now).Find(&models).Error; err != nil {
		return nil, err
	}
	sessions := make([]*domain.DeliverySession, len(models))
	for i, m := range models {
		sessions[i] = m.toDomain()
	}
	return sessions, nil
}

// SaveTarget 保存投递目标
func (r *DeliveryRepo) SaveTarget(t *domain.DeliveryTarget) error {
	return r.db.Create(toDeliveryTargetModel(t)).Error
}

// SaveTargets batch 保存投递目标
func (r *DeliveryRepo) SaveTargets(targets []*domain.DeliveryTarget) error {
	for _, t := range targets {
		if err := r.SaveTarget(t); err != nil {
			return err
		}
	}
	return nil
}

// UpdateTarget 更新投递目标状态
func (r *DeliveryRepo) UpdateTarget(t *domain.DeliveryTarget) error {
	return r.db.Save(toDeliveryTargetModel(t)).Error
}

// DeleteTargetsBySession 删除会话的所有投递目标
func (r *DeliveryRepo) DeleteTargetsBySession(sessionID string) error {
	return r.db.Where("session_id = ?", sessionID).Delete(&DeliveryTargetModel{}).Error
}
