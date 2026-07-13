package repository

import (
	"resumedesign/internal/domain"
	"time"

	"gorm.io/gorm"
)

// SectionModel GORM 模型
type SectionModel struct {
	ID          string `gorm:"primaryKey;size:36"`
	ResumeID    string `gorm:"not null;index:idx_sections_resume_id;index:idx_sections_resume_order,sort:asc"`
	SectionType string `gorm:"not null;size:20"`
	Title       string `gorm:"not null;default:''"`
	Content     string `gorm:"not null;default:''"`
	SortOrder   int    `gorm:"not null;default:0"`
	CreatedAt   time.Time `gorm:"not null"`
}

func (SectionModel) TableName() string { return "sections" }

func toSectionModel(s *domain.Section) *SectionModel {
	return &SectionModel{
		ID:          s.ID,
		ResumeID:    s.ResumeID,
		SectionType: string(s.Type),
		Title:       s.Title,
		Content:     s.Content,
		SortOrder:   s.Order,
	}
}

func (m *SectionModel) toDomain() *domain.Section {
	return &domain.Section{
		ID:      m.ID,
		ResumeID: m.ResumeID,
		Type:    domain.SectionType(m.SectionType),
		Title:   m.Title,
		Content: m.Content,
		Order:   m.SortOrder,
	}
}

// SectionRepo Section 仓储
type SectionRepo struct {
	db *gorm.DB
}

func NewSectionRepo(db *gorm.DB) *SectionRepo {
	return &SectionRepo{db: db}
}

func (r *SectionRepo) BatchCreate(sections []*domain.Section) error {
	models := make([]SectionModel, len(sections))
	for i, s := range sections {
		models[i] = *toSectionModel(s)
	}
	return r.db.Create(&models).Error
}

func (r *SectionRepo) FindByResumeID(resumeID string) ([]*domain.Section, error) {
	var models []SectionModel
	if err := r.db.Where("resume_id = ?", resumeID).
		Order("sort_order ASC").
		Find(&models).Error; err != nil {
		return nil, err
	}
	sections := make([]*domain.Section, len(models))
	for i, m := range models {
		sections[i] = m.toDomain()
	}
	return sections, nil
}

func (r *SectionRepo) DeleteByResumeID(resumeID string) error {
	return r.db.Where("resume_id = ?", resumeID).Delete(&SectionModel{}).Error
}
