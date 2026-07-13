package port

import "time"

// ConfigReader 配置读取器 Port 接口
// 不变量 T3：配置读取必须通过此接口，禁止 os.Getenv / flag 散落在业务代码中
//
// 降级策略：ConfigReader 实时查询 → LastKnownGood 缓存 → 硬编码默认值
// 实现：adapter/config/local.go（本地 YAML 文件）
//
// 后续接入 BlessStar：新增 adapter/config/blessstar.go 实现此接口即可
type ConfigReader interface {
	// GetString 获取字符串配置值
	GetString(key string) (string, error)
	// GetFloat 获取浮点配置值
	GetFloat(key string) (float64, error)
	// GetInt 获取整数配置值
	GetInt(key string) (int, error)
	// GetDuration 获取时间间隔配置值
	GetDuration(key string) (time.Duration, error)
	// Set 设置配置值（运行时生效）
	Set(key string, value interface{}) error
}

// 预定义配置键常量
const (
	ConfigLLMModel       = "llm.model"
	ConfigLLMTemperature = "llm.temperature"
	ConfigRewriteMaxLen  = "rewrite.max_length"
	ConfigCacheTTL       = "cache.ttl"
	ConfigJDTimeout      = "jd.crawl_timeout"
	ConfigSectionOrder   = "rewrite.section_order"
	ConfigExportFormats  = "export.format"

	// 岗位筛选配置
	ConfigScreeningTrustThreshold     = "screening.trust_threshold"
	ConfigScreeningMatchThreshold     = "screening.match_threshold"
	ConfigScreeningSalaryWeight       = "screening.salary_weight"
	ConfigScreeningBenefitWeight      = "screening.benefit_weight"
	ConfigScreeningIntroWeight        = "screening.intro_weight"
	ConfigScreeningLocationSameWeight = "screening.location_same_weight"
	ConfigScreeningLocationNearWeight = "screening.location_near_weight"
	ConfigScreeningVerifyWeight       = "screening.verify_weight"
	ConfigScreeningSearchTimeout      = "screening.search_timeout"

	// 批量投递配置
	ConfigDeliveryTTL             = "delivery.ttl"
	ConfigDeliveryCleanupInterval = "delivery.cleanup_interval"
	ConfigDeliveryMaxBatchSize    = "delivery.max_batch_size"
)
