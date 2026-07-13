package repository

import (
	"encoding/json"
	"time"

	"resumedesign/internal/domain"

	"gorm.io/gorm"
)

// ScoringSessionModel GORM 模型 — scoring_sessions 表
type ScoringSessionModel struct {
	ID             string     `gorm:"primaryKey;size:36"`
	Keywords       string     `gorm:"not null"`          // JSON 数组
	TrustThreshold float64    `gorm:"not null;default:0.6"`
	MatchThreshold float64    `gorm:"not null;default:0.6"`
	Status         string     `gorm:"not null;default:pending;size:20"`
	CreatedAt      time.Time  `gorm:"not null"`
	CompletedAt    *time.Time `gorm:"default:null"`
}

func (ScoringSessionModel) TableName() string { return "scoring_sessions" }

// ScoredJDModel GORM 模型 — scored_jds 表
type ScoredJDModel struct {
	ID                string    `gorm:"primaryKey;size:36"`
	SessionID         string    `gorm:"not null;index;size:36"`
	URL               string    `gorm:"default:''"`
	Company           string    `gorm:"not null;default:''"`
	Position          string    `gorm:"not null;default:''"`
	RawContent        string    `gorm:"not null;default:''"`
	TrustScore        float64   `gorm:"not null;default:0"`
	MatchScore        float64   `gorm:"not null;default:0"`
	IsPassed          bool      `gorm:"not null;default:false"`
	RejectReason      string    `gorm:"not null;default:''"`
	HasSalaryRange    bool      `gorm:"not null;default:false"`
	HasBenefits       bool      `gorm:"not null;default:false"`
	HasCompanyIntro   bool      `gorm:"not null;default:false"`
	LocationMatch     string    `gorm:"not null;default:'';size:20"`
	CompanyVerified   bool      `gorm:"not null;default:false"`
	MatchedKeywords   string    `gorm:"not null;default:''"`   // JSON 数组
	MatchedSynonyms   string    `gorm:"not null;default:''"`   // JSON 数组
	MatchedCategories string    `gorm:"not null;default:''"`   // JSON 数组
	CreatedAt         time.Time `gorm:"not null"`
}

func (ScoredJDModel) TableName() string { return "scored_jds" }

// ScoringRepo 岗位筛选数据仓储
type ScoringRepo struct {
	db *gorm.DB
}

func NewScoringRepo(db *gorm.DB) *ScoringRepo {
	return &ScoringRepo{db: db}
}

func toScoringSessionModel(s *domain.JDScoringSession) (*ScoringSessionModel, error) {
	keywordsJSON, err := json.Marshal(s.Keywords)
	if err != nil {
		return nil, err
	}
	return &ScoringSessionModel{
		ID:             s.ID,
		Keywords:       string(keywordsJSON),
		TrustThreshold: s.TrustThreshold,
		MatchThreshold: s.MatchThreshold,
		Status:         string(s.Status),
		CreatedAt:      s.CreatedAt,
		CompletedAt:    s.CompletedAt,
	}, nil
}

func (m *ScoringSessionModel) toDomain() (*domain.JDScoringSession, error) {
	var keywords []string
	if err := json.Unmarshal([]byte(m.Keywords), &keywords); err != nil {
		keywords = []string{}
	}
	return &domain.JDScoringSession{
		ID:             m.ID,
		Keywords:       keywords,
		TrustThreshold: m.TrustThreshold,
		MatchThreshold: m.MatchThreshold,
		Status:         domain.ScoringStatus(m.Status),
		CreatedAt:      m.CreatedAt,
		CompletedAt:    m.CompletedAt,
		Results:        make([]*domain.ScoredJD, 0),
	}, nil
}

func toScoredJDModel(jd *domain.ScoredJD) (*ScoredJDModel, error) {
	kwJSON, _ := json.Marshal(jd.Details.MatchedKeywords)
	synJSON, _ := json.Marshal(jd.Details.MatchedSynonyms)
	catJSON, _ := json.Marshal(jd.Details.MatchedCategories)
	return &ScoredJDModel{
		ID:                jd.ID,
		SessionID:         jd.ScoringSessionID,
		URL:               jd.URL,
		Company:           jd.Company,
		Position:          jd.Position,
		RawContent:        jd.RawContent,
		TrustScore:        jd.TrustScore,
		MatchScore:        jd.MatchScore,
		IsPassed:          jd.IsPassed,
		RejectReason:      jd.RejectReason,
		HasSalaryRange:    jd.Details.HasSalaryRange,
		HasBenefits:       jd.Details.HasBenefits,
		HasCompanyIntro:   jd.Details.HasCompanyIntro,
		LocationMatch:     jd.Details.LocationMatch,
		CompanyVerified:   jd.Details.CompanyVerified,
		MatchedKeywords:   string(kwJSON),
		MatchedSynonyms:   string(synJSON),
		MatchedCategories: string(catJSON),
		CreatedAt:         time.Now(),
	}, nil
}

func (m *ScoredJDModel) toDomain() *domain.ScoredJD {
	var kw, syn, cat []string
	json.Unmarshal([]byte(m.MatchedKeywords), &kw)
	json.Unmarshal([]byte(m.MatchedSynonyms), &syn)
	json.Unmarshal([]byte(m.MatchedCategories), &cat)

	return &domain.ScoredJD{
		ID:               m.ID,
		ScoringSessionID: m.SessionID,
		URL:              m.URL,
		Company:          m.Company,
		Position:         m.Position,
		RawContent:       m.RawContent,
		TrustScore:       m.TrustScore,
		MatchScore:       m.MatchScore,
		IsPassed:         m.IsPassed,
		RejectReason:     m.RejectReason,
		Details: domain.ScoreDetails{
			HasSalaryRange:    m.HasSalaryRange,
			HasBenefits:       m.HasBenefits,
			HasCompanyIntro:   m.HasCompanyIntro,
			LocationMatch:     m.LocationMatch,
			CompanyVerified:   m.CompanyVerified,
			MatchedKeywords:   kw,
			MatchedSynonyms:   syn,
			MatchedCategories: cat,
		},
	}
}

// CreateSession 创建筛选会话
func (r *ScoringRepo) CreateSession(s *domain.JDScoringSession) error {
	m, err := toScoringSessionModel(s)
	if err != nil {
		return err
	}
	return r.db.Create(m).Error
}

// FindSessionByID 查找筛选会话（含结果）
func (r *ScoringRepo) FindSessionByID(id string) (*domain.JDScoringSession, error) {
	var m ScoringSessionModel
	if err := r.db.First(&m, "id = ?", id).Error; err != nil {
		return nil, err
	}
	session, err := m.toDomain()
	if err != nil {
		return nil, err
	}

	// 加载评分结果
	var jdModels []ScoredJDModel
	r.db.Where("session_id = ?", id).Find(&jdModels)
	for i := range jdModels {
		session.Results = append(session.Results, jdModels[i].toDomain())
	}
	return session, nil
}

// UpdateSession 更新筛选会话状态
func (r *ScoringRepo) UpdateSession(s *domain.JDScoringSession) error {
	m, err := toScoringSessionModel(s)
	if err != nil {
		return err
	}
	return r.db.Save(m).Error
}

// SaveScoredJD 保存评分结果
func (r *ScoringRepo) SaveScoredJD(jd *domain.ScoredJD) error {
	m, err := toScoredJDModel(jd)
	if err != nil {
		return err
	}
	return r.db.Create(m).Error
}

// SaveScoredJD batch 批量保存评分结果
func (r *ScoringRepo) SaveScoredJDBatch(jds []*domain.ScoredJD) error {
	for _, jd := range jds {
		if err := r.SaveScoredJD(jd); err != nil {
			return err
		}
	}
	return nil
}

// FindPassedBySession 获取通过筛选的岗位
func (r *ScoringRepo) FindPassedBySession(sessionID string) ([]*domain.ScoredJD, error) {
	var models []ScoredJDModel
	if err := r.db.Where("session_id = ? AND is_passed = ?", sessionID, true).Find(&models).Error; err != nil {
		return nil, err
	}
	result := make([]*domain.ScoredJD, len(models))
	for i, m := range models {
		result[i] = m.toDomain()
	}
	return result, nil
}
