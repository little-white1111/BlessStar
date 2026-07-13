package repository

import (
	"resumedesign/internal/domain"
	"time"

	"gorm.io/gorm"
)

// DiffModel GORM 模型
type DiffModel struct {
	ID                string  `gorm:"primaryKey;size:36"`
	SessionID         string  `gorm:"not null;index:idx_diffs_session_id"`
	SectionType       string  `gorm:"not null;size:20"`
	OriginalContent   string  `gorm:"not null;default:''"`
	RewrittenContent  string  `gorm:"not null;default:''"`
	ChangeType        string  `gorm:"not null;size:10"`
	Confidence        float64 `gorm:"not null;default:1.0"`
	SortOrder         int     `gorm:"not null;default:0"`
	CreatedAt         time.Time `gorm:"not null"`
}

func (DiffModel) TableName() string { return "diffs" }

func toDiffModel(d *domain.Diff) *DiffModel {
	return &DiffModel{
		ID:               d.ID,
		SessionID:        d.SessionID,
		SectionType:      string(d.SectionType),
		OriginalContent:  d.OriginalContent,
		RewrittenContent: d.RewrittenContent,
		ChangeType:       string(d.ChangeType),
		Confidence:       d.Confidence,
		SortOrder:        d.SortOrder,
	}
}

func (m *DiffModel) toDomain() *domain.Diff {
	return &domain.Diff{
		ID:               m.ID,
		SessionID:        m.SessionID,
		SectionType:      domain.SectionType(m.SectionType),
		OriginalContent:  m.OriginalContent,
		RewrittenContent: m.RewrittenContent,
		ChangeType:       domain.ChangeType(m.ChangeType),
		Confidence:       m.Confidence,
		SortOrder:        m.SortOrder,
	}
}

// DiffRepo Diff 仓储
type DiffRepo struct {
	db *gorm.DB
}

func NewDiffRepo(db *gorm.DB) *DiffRepo {
	return &DiffRepo{db: db}
}

func (r *DiffRepo) BatchCreate(diffs []*domain.Diff) error {
	models := make([]DiffModel, len(diffs))
	for i, d := range diffs {
		models[i] = *toDiffModel(d)
	}
	return r.db.Create(&models).Error
}

func (r *DiffRepo) FindBySessionID(sessionID string) ([]*domain.Diff, error) {
	var models []DiffModel
	if err := r.db.Where("session_id = ?", sessionID).
		Order("sort_order ASC").
		Find(&models).Error; err != nil {
		return nil, err
	}
	diffs := make([]*domain.Diff, len(models))
	for i, m := range models {
		diffs[i] = m.toDomain()
	}
	return diffs, nil
}
