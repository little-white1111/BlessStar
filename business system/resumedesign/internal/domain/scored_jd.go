package domain

// ScoringStatus 筛选会话状态
type ScoringStatus string

const (
	ScoringPending   ScoringStatus = "pending"
	ScoringCompleted ScoringStatus = "completed"
	ScoringFailed    ScoringStatus = "failed"
)

// TargetStatus 投递目标状态
type TargetStatus string

const (
	TargetPending   TargetStatus = "pending"
	TargetExported  TargetStatus = "exported"
	TargetCompleted TargetStatus = "completed"
	TargetFailed    TargetStatus = "failed"
	TargetExpired   TargetStatus = "expired"
)

// DeliveryStatus 投递会话状态
type DeliveryStatus string

const (
	DeliveryPending   DeliveryStatus = "pending"
	DeliveryCompleted DeliveryStatus = "completed"
	DeliveryExpired   DeliveryStatus = "expired"
)

// ScoreDetails 评分明细值对象
type ScoreDetails struct {
	HasSalaryRange    bool
	HasBenefits       bool
	HasCompanyIntro   bool
	LocationMatch     string   // "same" | "near" | "different"
	CompanyVerified   bool
	MatchedKeywords   []string
	MatchedSynonyms   []string
	MatchedCategories []string
}

// ScoredJD 岗位评分实体
// 不变量 S1：RawContent 只读，评分过程不修改原始 JD 内容
// 不变量 S4：评分明细可追溯，未通过时必须携带 reject_reason
type ScoredJD struct {
	ID               string
	ScoringSessionID string
	URL              string
	Company          string
	Position         string
	RawContent       string
	TrustScore       float64
	MatchScore       float64
	IsPassed         bool
	RejectReason     string
	Details          ScoreDetails
}

// CalculateTrust 计算可信度总分
// 规则：薪资明确+0.1, 福利完整+0.1, 公司介绍+0.1,
//
//	位置相近+0.1/相同+0.2, 企业验证+0.2
func (s *ScoredJD) CalculateTrust(locationWeightNear, locationWeightSame, salaryWeight, benefitWeight, introWeight, verifyWeight float64) float64 {
	var score float64
	d := s.Details

	if d.HasSalaryRange {
		score += salaryWeight
	}
	if d.HasBenefits {
		score += benefitWeight
	}
	if d.HasCompanyIntro {
		score += introWeight
	}
	switch d.LocationMatch {
	case "same":
		score += locationWeightSame
	case "near":
		score += locationWeightNear
	}
	if d.CompanyVerified {
		score += verifyWeight
	}

	if score > 1.0 {
		score = 1.0
	}
	s.TrustScore = score
	return score
}

// CalculateMatch 计算关键词匹配度
func (s *ScoredJD) CalculateMatch(totalKeywords int) float64 {
	if totalKeywords == 0 {
		s.MatchScore = 0
		return 0
	}

	matched := len(s.Details.MatchedKeywords) + len(s.Details.MatchedSynonyms) + len(s.Details.MatchedCategories)
	// 避免匹配数超过关键词总数的极端情况
	if matched > totalKeywords {
		matched = totalKeywords
	}
	s.MatchScore = float64(matched) / float64(totalKeywords)
	return s.MatchScore
}

// IsValidScoringStatus 判断筛选状态是否合法
func IsValidScoringStatus(s ScoringStatus) bool {
	switch s {
	case ScoringPending, ScoringCompleted, ScoringFailed:
		return true
	}
	return false
}

// IsValidTargetStatus 判断目标状态是否合法
func IsValidTargetStatus(s TargetStatus) bool {
	switch s {
	case TargetPending, TargetExported, TargetCompleted, TargetFailed, TargetExpired:
		return true
	}
	return false
}

// IsValidDeliveryStatus 判断投递状态是否合法
func IsValidDeliveryStatus(s DeliveryStatus) bool {
	switch s {
	case DeliveryPending, DeliveryCompleted, DeliveryExpired:
		return true
	}
	return false
}
