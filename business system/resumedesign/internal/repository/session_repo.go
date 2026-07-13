package repository

import (
	"resumedesign/internal/domain"
	"time"

	"gorm.io/gorm"
)

// RewriteSessionModel GORM 模型
type RewriteSessionModel struct {
	ID               string     `gorm:"primaryKey;size:36"`
	ResumeID         string     `gorm:"not null;index:idx_sessions_resume_id"`
	JdID             string     `gorm:"not null"`
	Status           string     `gorm:"not null;size:15;index:idx_sessions_status"`
	Model            string     `gorm:"not null;default:''"`
	Temperature      float64    `gorm:"not null;default:0.7"`
	RewrittenContent string     `gorm:"not null;default:''"`
	PromptHash       string     `gorm:"not null;default:''"`
	ErrorMessage     string     `gorm:"not null;default:''"`
	CreatedAt        time.Time  `gorm:"not null"`
	CompletedAt      *time.Time
}

func (RewriteSessionModel) TableName() string { return "rewrite_sessions" }

func toSessionModel(s *domain.RewriteSession) *RewriteSessionModel {
	return &RewriteSessionModel{
		ID:               s.ID,
		ResumeID:         s.ResumeID,
		JdID:             s.JdID,
		Status:           string(s.Status),
		Model:            s.Model,
		Temperature:      s.Temperature,
		RewrittenContent: s.RewrittenContent,
		PromptHash:       s.PromptHash,
		ErrorMessage:     s.ErrorMessage,
		CreatedAt:        s.CreatedAt,
		CompletedAt:      s.CompletedAt,
	}
}

func (m *RewriteSessionModel) toDomain() *domain.RewriteSession {
	return &domain.RewriteSession{
		ID:               m.ID,
		ResumeID:         m.ResumeID,
		JdID:             m.JdID,
		Status:           domain.SessionStatus(m.Status),
		Model:            m.Model,
		Temperature:      m.Temperature,
		RewrittenContent: m.RewrittenContent,
		PromptHash:       m.PromptHash,
		ErrorMessage:     m.ErrorMessage,
		CreatedAt:        m.CreatedAt,
		CompletedAt:      m.CompletedAt,
	}
}

// SessionRepo RewriteSession 仓储
type SessionRepo struct {
	db *gorm.DB
}

func NewSessionRepo(db *gorm.DB) *SessionRepo {
	return &SessionRepo{db: db}
}

func (r *SessionRepo) Create(s *domain.RewriteSession) error {
	return r.db.Create(toSessionModel(s)).Error
}

func (r *SessionRepo) FindByID(id string) (*domain.RewriteSession, error) {
	var m RewriteSessionModel
	if err := r.db.First(&m, "id = ?", id).Error; err != nil {
		return nil, err
	}
	return m.toDomain(), nil
}

func (r *SessionRepo) FindAll() ([]*domain.RewriteSession, error) {
	var models []RewriteSessionModel
	if err := r.db.Order("created_at DESC").Find(&models).Error; err != nil {
		return nil, err
	}
	sessions := make([]*domain.RewriteSession, len(models))
	for i, m := range models {
		sessions[i] = m.toDomain()
	}
	return sessions, nil
}

func (r *SessionRepo) Update(s *domain.RewriteSession) error {
	return r.db.Model(&RewriteSessionModel{}).
		Where("id = ?", s.ID).
		Updates(toSessionModel(s)).Error
}
