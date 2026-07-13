package repository

import (
	"resumedesign/internal/domain"
	"time"

	"gorm.io/gorm"
)

// ResumeModel GORM 模型
type ResumeModel struct {
	ID           string    `gorm:"primaryKey;size:36"`
	Title        string    `gorm:"not null"`
	RawContent   string    `gorm:"not null"`
	ImportFormat string    `gorm:"not null;size:10"`
	CreatedAt    time.Time `gorm:"not null"`
	UpdatedAt    time.Time `gorm:"not null"`
}

func (ResumeModel) TableName() string { return "resumes" }

func toResumeModel(r *domain.Resume) *ResumeModel {
	return &ResumeModel{
		ID:           r.ID,
		Title:        r.Title,
		RawContent:   r.RawContent,
		ImportFormat: r.ImportFormat,
		CreatedAt:    r.CreatedAt,
		UpdatedAt:    r.UpdatedAt,
	}
}

func (m *ResumeModel) toDomain() *domain.Resume {
	return &domain.Resume{
		ID:           m.ID,
		Title:        m.Title,
		RawContent:   m.RawContent,
		ImportFormat: m.ImportFormat,
		CreatedAt:    m.CreatedAt,
		UpdatedAt:    m.UpdatedAt,
	}
}

// ResumeRepo Resume 仓储
type ResumeRepo struct {
	db *gorm.DB
}

func NewResumeRepo(db *gorm.DB) *ResumeRepo {
	return &ResumeRepo{db: db}
}

func (r *ResumeRepo) Create(resume *domain.Resume) error {
	return r.db.Create(toResumeModel(resume)).Error
}

func (r *ResumeRepo) FindByID(id string) (*domain.Resume, error) {
	var m ResumeModel
	if err := r.db.First(&m, "id = ?", id).Error; err != nil {
		return nil, err
	}
	return m.toDomain(), nil
}

func (r *ResumeRepo) FindAll() ([]*domain.Resume, error) {
	var models []ResumeModel
	if err := r.db.Order("created_at DESC").Find(&models).Error; err != nil {
		return nil, err
	}
	resumes := make([]*domain.Resume, len(models))
	for i, m := range models {
		resumes[i] = m.toDomain()
	}
	return resumes, nil
}

func (r *ResumeRepo) Delete(id string) error {
	return r.db.Delete(&ResumeModel{}, "id = ?", id).Error
}
