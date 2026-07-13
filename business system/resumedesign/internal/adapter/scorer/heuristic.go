package scorer

import (
	"regexp"
	"strings"

	"resumedesign/internal/port"
)

// HeuristicScorer 启发式可信度评分实现
// 基于 JD 文本内容判断薪资明确性、福利完整性、公司介绍、位置匹配等因子
type HeuristicScorer struct {
	userLocation string
}

func NewHeuristicScorer(userLocation string) *HeuristicScorer {
	return &HeuristicScorer{userLocation: userLocation}
}

func (s *HeuristicScorer) Score(jd *port.FetchedJD, location string) (*port.TrustScore, error) {
	content := jd.RawContent

	score := &port.TrustScore{
		Details: port.TrustDetails{
			HasSalaryRange:  detectSalaryRange(content),
			HasBenefits:     detectBenefits(content),
			HasCompanyIntro: detectCompanyIntro(content),
			LocationMatch:   detectLocationMatch(content, location),
			CompanyVerified: false, // 需要第三方 API，默认 false
		},
	}

	// 计算各项分数
	if score.Details.HasSalaryRange {
		score.SalaryScore = 0.1
	}
	if score.Details.HasBenefits {
		score.BenefitScore = 0.1
	}
	if score.Details.HasCompanyIntro {
		score.IntroScore = 0.1
	}
	switch score.Details.LocationMatch {
	case "same":
		score.LocationScore = 0.2
	case "near":
		score.LocationScore = 0.1
	}
	if score.Details.CompanyVerified {
		score.Verified = true
	}

	score.Score = score.SalaryScore + score.BenefitScore + score.IntroScore + score.LocationScore
	if score.Details.CompanyVerified {
		score.Score += 0.2
	}
	if score.Score > 1.0 {
		score.Score = 1.0
	}

	return score, nil
}

// detectSalaryRange 检测 JD 文本中是否包含具体薪资范围
// 匹配模式：薪资/工资/月薪/年薪 + 数字范围（如 15k-25k、10K-20K、15万-30万）
func detectSalaryRange(content string) bool {
	patterns := []string{
		`[薪資资酬].*?[0-9]+[kK万]\s*[-~至到]\s*[0-9]+[kK万]`,
		`[0-9]+[kK万]\s*[-~至到]\s*[0-9]+[kK万].*?[薪資资酬]`,
		`月薪[：:]\s*[0-9]+`,
		`年薪[：:]\s*[0-9]+`,
		`[0-9]+K[-~][0-9]+K`,
		`[0-9]+\.?\d*[kK]\s*[-~]\s*[0-9]+\.?\d*[kK]`,
	}
	for _, p := range patterns {
		if matched, _ := regexp.MatchString(p, content); matched {
			return true
		}
	}
	return false
}

// detectBenefits 检测 JD 文本中是否包含福利信息
func detectBenefits(content string) bool {
	keywords := []string{
		"五险一金", "社保", "公积金", "住房补贴",
		"餐补", "交通补贴", "通讯补贴", "年终奖",
		"带薪年假", "定期体检", "补充医疗", "股票期权",
		"弹性工作", "周末双休",
	}
	contentLower := strings.ToLower(content)
	for _, kw := range keywords {
		if strings.Contains(contentLower, strings.ToLower(kw)) {
			return true
		}
	}
	return false
}

// detectCompanyIntro 检测 JD 文本中是否包含公司介绍段落
func detectCompanyIntro(content string) bool {
	patterns := []string{
		`(?i)公司简介`,
		`(?i)关于我们`,
		`(?i)公司介绍`,
		`(?i)企业简介`,
		`(?i)我们是一家`,
		`(?i)成立于`,
	}
	for _, p := range patterns {
		if matched, _ := regexp.MatchString(p, content); matched {
			return true
		}
	}
	return false
}

// detectLocationMatch 检测 JD 文本中的位置信息与用户位置是否匹配
func detectLocationMatch(content string, userLocation string) string {
	if userLocation == "" {
		return "different"
	}

	contentLower := strings.ToLower(content)
	userLocLower := strings.ToLower(userLocation)

	// 位置模式：工作地点/上班地点/办公地点/所在城市 + 具体位置
	locPatterns := []string{
		`工作地点[：:]\s*([^\n]+)`,
		`上班地点[：:]\s*([^\n]+)`,
		`办公地点[：:]\s*([^\n]+)`,
		`所在城市[：:]\s*([^\n]+)`,
		`工作地址[：:]\s*([^\n]+)`,
	}

	for _, p := range locPatterns {
		re := regexp.MustCompile(p)
		matches := re.FindStringSubmatch(content)
		if len(matches) > 1 {
			loc := strings.TrimSpace(matches[1])
			locLower := strings.ToLower(loc)
			if strings.Contains(locLower, userLocLower) {
				return "same"
			}
			// 简单相近判断：前4个字符匹配（如"北京"→"北京海淀"）
			if len(userLocLower) >= 2 && strings.Contains(locLower, userLocLower[:2]) {
				return "near"
			}
		}
	}

	// 如果全文包含用户城市名，标记为 near
	if strings.Contains(contentLower, userLocLower) {
		return "near"
	}

	return "different"
}
