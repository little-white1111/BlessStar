package service

import (
	"context"
	"fmt"
	"time"

	"resumedesign/internal/domain"
	"resumedesign/internal/port"
	"resumedesign/internal/repository"

	"github.com/google/uuid"
)

// ScreeningService 岗位筛选编排服务
type ScreeningService struct {
	scorer      port.TrustScorer
	expander    port.KeywordExpander
	searchEng   port.JDSearchEngine
	fetcher     port.JDFetcher
	config      port.ConfigReader
	scoringRepo *repository.ScoringRepo
}

func NewScreeningService(
	scorer port.TrustScorer,
	expander port.KeywordExpander,
	searchEng port.JDSearchEngine,
	fetcher port.JDFetcher,
	config port.ConfigReader,
	scoringRepo *repository.ScoringRepo,
) *ScreeningService {
	return &ScreeningService{
		scorer:      scorer,
		expander:    expander,
		searchEng:   searchEng,
		fetcher:     fetcher,
		config:      config,
		scoringRepo: scoringRepo,
	}
}

// DeliveryTargetInput 投递目标输入
type DeliveryTargetInput struct {
	ScoredJDID string
	Company    string
	Position   string
	URL        string
}

// StartScoringSession 启动筛选会话
// sourceType: "search" | "urls"
func (s *ScreeningService) StartScoringSession(
	keywords []string,
	sourceType string,
	urls []string,
	trustThreshold, matchThreshold float64,
) (*domain.JDScoringSession, error) {
	if len(keywords) == 0 && sourceType == "search" {
		return nil, fmt.Errorf("keywords required for search mode")
	}
	if len(urls) == 0 && sourceType == "urls" {
		return nil, fmt.Errorf("urls required for url mode")
	}

	session := &domain.JDScoringSession{
		ID:             uuid.New().String(),
		Keywords:       keywords,
		TrustThreshold: trustThreshold,
		MatchThreshold: matchThreshold,
		Status:         domain.ScoringPending,
		CreatedAt:      time.Now(),
		Results:        make([]*domain.ScoredJD, 0),
	}

	if err := s.scoringRepo.CreateSession(session); err != nil {
		return nil, fmt.Errorf("create scoring session failed: %w", err)
	}

	return session, nil
}

// RunScreening 执行筛选
func (s *ScreeningService) RunScreening(ctx context.Context, sessionID string) error {
	session, err := s.scoringRepo.FindSessionByID(sessionID)
	if err != nil {
		return fmt.Errorf("session not found: %s", sessionID)
	}

	if session.Status != domain.ScoringPending {
		return fmt.Errorf("session %s is not pending (status: %s)", sessionID, session.Status)
	}

	// 1. 扩展关键词
	expanded, err := s.expander.Expand(session.Keywords)
	if err != nil {
		session.MarkFailed()
		s.scoringRepo.UpdateSession(session)
		return fmt.Errorf("keyword expansion failed: %w", err)
	}

	// 2. 获取 JD 列表
	// 注意：实际实现需要从 session 获取 source 信息
	// 此处假设从 Search 获取；URL 模式可在外部先获取 JD 后再调用评分
	jdLinks, err := s.searchEng.Search(session.Keywords, 1)
	if err != nil {
		session.MarkFailed()
		s.scoringRepo.UpdateSession(session)
		return fmt.Errorf("search JD failed: %w", err)
	}

	// 3. 逐 JD 评分
	for _, link := range jdLinks {
		select {
		case <-ctx.Done():
			session.MarkFailed()
			s.scoringRepo.UpdateSession(session)
			return ctx.Err()
		default:
		}

		// 获取 JD 详情
		fetchedJD, err := s.searchEng.FetchDetail(link.URL)
		if err != nil {
			continue // 单个失败跳过
		}

		scoredJD := &domain.ScoredJD{
			ID:         uuid.New().String(),
			URL:        link.URL,
			Company:    fetchedJD.Company,
			Position:   fetchedJD.Position,
			RawContent: fetchedJD.RawContent,
		}

		// 可信度评分
		trustScore, err := s.scorer.Score(fetchedJD, "")
		if err == nil {
			scoredJD.Details = domain.ScoreDetails{
				HasSalaryRange:    trustScore.Details.HasSalaryRange,
				HasBenefits:       trustScore.Details.HasBenefits,
				HasCompanyIntro:   trustScore.Details.HasCompanyIntro,
				LocationMatch:     trustScore.Details.LocationMatch,
				CompanyVerified:   trustScore.Details.CompanyVerified,
				MatchedKeywords:   make([]string, 0),
				MatchedSynonyms:   make([]string, 0),
				MatchedCategories: make([]string, 0),
			}
			scoredJD.CalculateTrust(0.1, 0.2, 0.1, 0.1, 0.1, 0.2)
		}

		// 关键词匹配度评分
		matchDetail := s.expander.MatchDetail(fetchedJD.Position, fetchedJD.RawContent, expanded)
		scoredJD.Details.MatchedKeywords = append(matchDetail.TitleMatches, matchDetail.ContentMatches...)
		scoredJD.Details.MatchedSynonyms = matchDetail.SynonymMatches
		scoredJD.Details.MatchedCategories = matchDetail.CategoryMatches
		scoredJD.MatchScore = matchDetail.TotalScore

		// 加入会话
		session.AddResult(scoredJD)
		s.scoringRepo.SaveScoredJD(scoredJD)
	}

	session.MarkCompleted()
	return s.scoringRepo.UpdateSession(session)
}

// GetScreeningResult 获取筛选结果
func (s *ScreeningService) GetScreeningResult(sessionID string) (*domain.JDScoringSession, error) {
	return s.scoringRepo.FindSessionByID(sessionID)
}

// GetPassedResults 仅获取通过岗位
func (s *ScreeningService) GetPassedResults(sessionID string) ([]*domain.ScoredJD, error) {
	return s.scoringRepo.FindPassedBySession(sessionID)
}
