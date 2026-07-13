package service

import (
	"fmt"
	"time"

	"resumedesign/internal/domain"
	"resumedesign/internal/port"
	"resumedesign/internal/repository"

	"github.com/google/uuid"
)

// JDService JD 解析用例
type JDService struct {
	jdRepo  *repository.JDRepo
	fetcher port.JDFetcher
}

func NewJDService(jdRepo *repository.JDRepo, fetcher port.JDFetcher) *JDService {
	return &JDService{
		jdRepo:  jdRepo,
		fetcher: fetcher,
	}
}

// ParseFromURL 从 URL 解析 JD
// 不变量 7：JD 来源 URL 必须经过有效性校验，失败时降级为用户手动粘贴
func (s *JDService) ParseFromURL(url string, timeout time.Duration) (*domain.JobDescription, error) {
	// 通过 Port 接口抓取（不变量 T2：不依赖具体 adapter 实现）
	fetched, err := s.fetcher.FetchFromURL(url, timeout)
	if err != nil {
		return nil, fmt.Errorf("fetch JD from URL failed: %w", err)
	}

	jd := &domain.JobDescription{
		ID:         uuid.New().String(),
		Source:     "url",
		SourceURL:  url,
		Company:    fetched.Company,
		Position:   fetched.Position,
		RawContent: fetched.RawContent,
		CreatedAt:  time.Now(),
	}

	if err := s.jdRepo.Create(jd); err != nil {
		return nil, fmt.Errorf("save JD failed: %w", err)
	}

	return jd, nil
}

// ParseFromText 从文本解析 JD
func (s *JDService) ParseFromText(text string) (*domain.JobDescription, error) {
	jd := &domain.JobDescription{
		ID:         uuid.New().String(),
		Source:     "manual",
		RawContent: text,
		CreatedAt:  time.Now(),
	}

	if err := s.jdRepo.Create(jd); err != nil {
		return nil, fmt.Errorf("save JD failed: %w", err)
	}

	return jd, nil
}

// List 获取 JD 列表
func (s *JDService) List() ([]*domain.JobDescription, error) {
	return s.jdRepo.FindAll()
}

// GetByID 获取 JD 详情
func (s *JDService) GetByID(id string) (*domain.JobDescription, error) {
	jd, err := s.jdRepo.FindByID(id)
	if err != nil {
		return nil, fmt.Errorf("JD not found: %s", id)
	}
	return jd, nil
}
