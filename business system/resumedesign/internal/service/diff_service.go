package service

import (
	"fmt"

	"resumedesign/internal/domain"
	"resumedesign/internal/repository"
)

// DiffService 差异预览用例
type DiffService struct {
	diffRepo *repository.DiffRepo
}

func NewDiffService(diffRepo *repository.DiffRepo) *DiffService {
	return &DiffService{
		diffRepo: diffRepo,
	}
}

// GetDiffBySession 获取会话的差异列表
func (s *DiffService) GetDiffBySession(sessionID string) ([]*domain.Diff, error) {
	diffs, err := s.diffRepo.FindBySessionID(sessionID)
	if err != nil {
		return nil, fmt.Errorf("get diffs failed: %w", err)
	}
	if len(diffs) == 0 {
		return nil, fmt.Errorf("no diffs found for session: %s", sessionID)
	}
	return diffs, nil
}
