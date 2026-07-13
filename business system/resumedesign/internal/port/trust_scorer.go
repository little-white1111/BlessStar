package port

// TrustScore 可信度评分结果
type TrustScore struct {
	Score         float64
	SalaryScore   float64
	BenefitScore  float64
	IntroScore    float64
	LocationScore float64
	Verified      bool
	Details       TrustDetails
}

// TrustDetails 可信度评分因子明细
type TrustDetails struct {
	HasSalaryRange  bool
	HasBenefits     bool
	HasCompanyIntro bool
	LocationMatch   string // "same" | "near" | "different"
	CompanyVerified bool
}

// TrustScorer 可信度评分 Port 接口
// 职责：基于 JD 文本分析和规则计算岗位可信度
type TrustScorer interface {
	// Score 对 JD 进行可信度评分，location 为用户所在城市
	Score(jd *FetchedJD, location string) (*TrustScore, error)
}

// CompanyVerifier 企业验证 Port 接口
type CompanyVerifier interface {
	// Verify 验证企业信息
	Verify(companyName string, location string) (*CompanyInfo, error)
}

// CompanyInfo 企业信息
type CompanyInfo struct {
	Name     string
	Location string
	Verified bool
	Credit   string
}
