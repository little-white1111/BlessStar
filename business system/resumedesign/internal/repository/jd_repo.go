package repository

import (
	"resumedesign/internal/domain"
	"time"

	"gorm.io/gorm"
)

// JobDescriptionModel GORM 模型
type JobDescriptionModel struct {
	ID         string    `gorm:"primaryKey;size:36"`
	Source     string    `gorm:"not null;size:10"`
	SourceURL  string    `gorm:"default:''"`
	Company    string    `gorm:"not null;default:''"`
	Position   string    `gorm:"not null;default:''"`
	RawContent string    `gorm:"not null"`
	CreatedAt  time.Time `gorm:"not null"`
}

func (JobDescriptionModel) TableName() string { return "job_descriptions" }

func toJDModel(jd *domain.JobDescription) *JobDescriptionModel {
	return &JobDescriptionModel{
		ID:         jd.ID,
		Source:     jd.Source,
		SourceURL:  jd.SourceURL,
		Company:    jd.Company,
		Position:   jd.Position,
		RawContent: jd.RawContent,
		CreatedAt:  jd.CreatedAt,
	}
}

func (m *JobDescriptionModel) toDomain() *domain.JobDescription {
	return &domain.JobDescription{
		ID:         m.ID,
		Source:     m.Source,
		SourceURL:  m.SourceURL,
		Company:    m.Company,
		Position:   m.Position,
		RawContent: m.RawContent,
		CreatedAt:  m.CreatedAt,
	}
}

// JDRepo JobDescription 仓储
type JDRepo struct {
	db *gorm.DB
}

func NewJDRepo(db *gorm.DB) *JDRepo {
	return &JDRepo{db: db}
}

func (r *JDRepo) Create(jd *domain.JobDescription) error {
	return r.db.Create(toJDModel(jd)).Error
}

func (r *JDRepo) FindByID(id string) (*domain.JobDescription, error) {
	var m JobDescriptionModel
	if err := r.db.First(&m, "id = ?", id).Error; err != nil {
		return nil, err
	}
	return m.toDomain(), nil
}

func (r *JDRepo) FindAll() ([]*domain.JobDescription, error) {
	var models []JobDescriptionModel
	if err := r.db.Order("created_at DESC").Find(&models).Error; err != nil {
		return nil, err
	}
	jds := make([]*domain.JobDescription, len(models))
	for i, m := range models {
		jds[i] = m.toDomain()
	}
	return jds, nil
}
