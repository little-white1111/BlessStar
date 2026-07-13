package domain

import (
	"fmt"
	"time"
)

// JDScoringSession 岗位筛选会话聚合根
// 不变量 S3：通过和未通过严格基于阈值，不可人工干预
type JDScoringSession struct {
	ID             string
	Keywords       []string
	TrustThreshold float64
	MatchThreshold float64
	Status         ScoringStatus
	Results        []*ScoredJD
	CreatedAt      time.Time
	CompletedAt    *time.Time
}

// AddResult 添加评分结果
func (s *JDScoringSession) AddResult(jd *ScoredJD) {
	jd.ScoringSessionID = s.ID
	jd.IsPassed = jd.TrustScore >= s.TrustThreshold && jd.MatchScore >= s.MatchThreshold

	if !jd.IsPassed && jd.RejectReason == "" {
		jd.RejectReason = s.buildRejectReason(jd)
	}

	s.Results = append(s.Results, jd)
}

// GetPassedResults 获取通过岗位
func (s *JDScoringSession) GetPassedResults() []*ScoredJD {
	passed := make([]*ScoredJD, 0, len(s.Results))
	for _, jd := range s.Results {
		if jd.IsPassed {
			passed = append(passed, jd)
		}
	}
	return passed
}

// GetRejectedResults 获取未通过岗位及原因
func (s *JDScoringSession) GetRejectedResults() []*ScoredJD {
	rejected := make([]*ScoredJD, 0, len(s.Results))
	for _, jd := range s.Results {
		if !jd.IsPassed {
			rejected = append(rejected, jd)
		}
	}
	return rejected
}

// MarkCompleted 标记完成
func (s *JDScoringSession) MarkCompleted() {
	s.Status = ScoringCompleted
	now := time.Now()
	s.CompletedAt = &now
}

// MarkFailed 标记失败
func (s *JDScoringSession) MarkFailed() {
	s.Status = ScoringFailed
}

// buildRejectReason 构建未通过原因
func (s *JDScoringSession) buildRejectReason(jd *ScoredJD) string {
	reason := ""
	if jd.TrustScore < s.TrustThreshold {
		reason += "trust_score=" + formatScore(jd.TrustScore) + "<threshold=" + formatScore(s.TrustThreshold)
	}
	if jd.MatchScore < s.MatchThreshold {
		if reason != "" {
			reason += "; "
		}
		reason += "match_score=" + formatScore(jd.MatchScore) + "<threshold=" + formatScore(s.MatchThreshold)
	}
	if reason == "" {
		reason = "unknown"
	}
	return reason
}

func formatScore(s float64) string {
	if s == float64(int64(s)) {
		return fmt.Sprintf("%.0f", s)
	}
	return fmt.Sprintf("%.2f", s)
}
