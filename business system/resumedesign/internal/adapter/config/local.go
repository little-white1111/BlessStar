package config

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"resumedesign/internal/port"
	"gopkg.in/yaml.v3"
)

// ConfigModel 本地配置文件模型
type ConfigModel struct {
	LLM struct {
		Model       string  `yaml:"model"`
		Temperature float64 `yaml:"temperature"`
	} `yaml:"llm"`
	Rewrite struct {
		MaxLength    int      `yaml:"max_length"`
		SectionOrder []string `yaml:"section_order"`
	} `yaml:"rewrite"`
	Cache struct {
		TTL string `yaml:"ttl"` // e.g. "24h"
	} `yaml:"cache"`
	Export struct {
		Formats []string `yaml:"formats"`
	} `yaml:"export"`
	JD struct {
		CrawlTimeout string `yaml:"crawl_timeout"` // e.g. "10s"
	} `yaml:"jd"`
	Screening struct {
		TrustThreshold     float64 `yaml:"trust_threshold"`
		MatchThreshold     float64 `yaml:"match_threshold"`
		SalaryWeight       float64 `yaml:"salary_weight"`
		BenefitWeight      float64 `yaml:"benefit_weight"`
		IntroWeight        float64 `yaml:"intro_weight"`
		LocationSameWeight float64 `yaml:"location_same_weight"`
		LocationNearWeight float64 `yaml:"location_near_weight"`
		VerifyWeight       float64 `yaml:"verify_weight"`
		SearchTimeout      string  `yaml:"search_timeout"`
	} `yaml:"screening"`
	Delivery struct {
		TTL             string `yaml:"ttl"`
		CleanupInterval string `yaml:"cleanup_interval"`
		MaxBatchSize    int    `yaml:"max_batch_size"`
	} `yaml:"delivery"`
}

// DefaultConfig 默认配置
func DefaultConfig() *ConfigModel {
	return &ConfigModel{
		LLM: struct {
			Model       string  `yaml:"model"`
			Temperature float64 `yaml:"temperature"`
		}{
			Model:       "gpt-4o",
			Temperature: 0.7,
		},
		Rewrite: struct {
			MaxLength   int      `yaml:"max_length"`
			SectionOrder []string `yaml:"section_order"`
		}{
			MaxLength: 1000,
			SectionOrder: []string{
				"个人信息", "教育背景", "工作经历", "项目经历", "技能",
			},
		},
		Cache: struct {
			TTL string `yaml:"ttl"`
		}{
			TTL: "24h",
		},
		Export: struct {
			Formats []string `yaml:"formats"`
		}{
			Formats: []string{"pdf", "docx"},
		},
		JD: struct {
			CrawlTimeout string `yaml:"crawl_timeout"`
		}{
			CrawlTimeout: "10s",
		},
		Screening: struct {
			TrustThreshold     float64 `yaml:"trust_threshold"`
			MatchThreshold     float64 `yaml:"match_threshold"`
			SalaryWeight       float64 `yaml:"salary_weight"`
			BenefitWeight      float64 `yaml:"benefit_weight"`
			IntroWeight        float64 `yaml:"intro_weight"`
			LocationSameWeight float64 `yaml:"location_same_weight"`
			LocationNearWeight float64 `yaml:"location_near_weight"`
			VerifyWeight       float64 `yaml:"verify_weight"`
			SearchTimeout      string  `yaml:"search_timeout"`
		}{
			TrustThreshold:     0.6,
			MatchThreshold:     0.6,
			SalaryWeight:       0.1,
			BenefitWeight:      0.1,
			IntroWeight:        0.1,
			LocationSameWeight: 0.2,
			LocationNearWeight: 0.1,
			VerifyWeight:       0.2,
			SearchTimeout:      "30s",
		},
		Delivery: struct {
			TTL             string `yaml:"ttl"`
			CleanupInterval string `yaml:"cleanup_interval"`
			MaxBatchSize    int    `yaml:"max_batch_size"`
		}{
			TTL:             "168h",
			CleanupInterval: "1h",
			MaxBatchSize:    50,
		},
	}
}

// LocalConfigReader 本地 YAML 文件配置读取器
// 配置文件路径: ~/.resumedesign/config.yaml
type LocalConfigReader struct {
	mu       sync.RWMutex
	config   *ConfigModel
	filePath string
}

func NewLocalConfigReader() *LocalConfigReader {
	home, _ := os.UserHomeDir()
	path := filepath.Join(home, ".resumedesign", "config.yaml")

	r := &LocalConfigReader{
		config:   DefaultConfig(),
		filePath: path,
	}
	r.load()
	return r
}

func (r *LocalConfigReader) load() {
	data, err := os.ReadFile(r.filePath)
	if err != nil {
		return // 使用默认配置
	}

	var cfg ConfigModel
	if err := yaml.Unmarshal(data, &cfg); err != nil {
		return
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	// 合并配置（保留默认值中未设置的字段）
	if cfg.LLM.Model != "" {
		r.config.LLM.Model = cfg.LLM.Model
	}
	if cfg.LLM.Temperature > 0 {
		r.config.LLM.Temperature = cfg.LLM.Temperature
	}
	if cfg.Rewrite.MaxLength > 0 {
		r.config.Rewrite.MaxLength = cfg.Rewrite.MaxLength
	}
	if len(cfg.Rewrite.SectionOrder) > 0 {
		r.config.Rewrite.SectionOrder = cfg.Rewrite.SectionOrder
	}
	if cfg.Cache.TTL != "" {
		r.config.Cache.TTL = cfg.Cache.TTL
	}
	if len(cfg.Export.Formats) > 0 {
		r.config.Export.Formats = cfg.Export.Formats
	}
	if cfg.JD.CrawlTimeout != "" {
		r.config.JD.CrawlTimeout = cfg.JD.CrawlTimeout
	}
	if cfg.Screening.TrustThreshold > 0 {
		r.config.Screening.TrustThreshold = cfg.Screening.TrustThreshold
	}
	if cfg.Screening.MatchThreshold > 0 {
		r.config.Screening.MatchThreshold = cfg.Screening.MatchThreshold
	}
	if cfg.Screening.SalaryWeight > 0 {
		r.config.Screening.SalaryWeight = cfg.Screening.SalaryWeight
	}
	if cfg.Screening.BenefitWeight > 0 {
		r.config.Screening.BenefitWeight = cfg.Screening.BenefitWeight
	}
	if cfg.Screening.IntroWeight > 0 {
		r.config.Screening.IntroWeight = cfg.Screening.IntroWeight
	}
	if cfg.Screening.LocationSameWeight > 0 {
		r.config.Screening.LocationSameWeight = cfg.Screening.LocationSameWeight
	}
	if cfg.Screening.LocationNearWeight > 0 {
		r.config.Screening.LocationNearWeight = cfg.Screening.LocationNearWeight
	}
	if cfg.Screening.VerifyWeight > 0 {
		r.config.Screening.VerifyWeight = cfg.Screening.VerifyWeight
	}
	if cfg.Screening.SearchTimeout != "" {
		r.config.Screening.SearchTimeout = cfg.Screening.SearchTimeout
	}
	if cfg.Delivery.TTL != "" {
		r.config.Delivery.TTL = cfg.Delivery.TTL
	}
	if cfg.Delivery.CleanupInterval != "" {
		r.config.Delivery.CleanupInterval = cfg.Delivery.CleanupInterval
	}
	if cfg.Delivery.MaxBatchSize > 0 {
		r.config.Delivery.MaxBatchSize = cfg.Delivery.MaxBatchSize
	}
}

func (r *LocalConfigReader) save() error {
	r.mu.RLock()
	data, err := yaml.Marshal(r.config)
	r.mu.RUnlock()
	if err != nil {
		return err
	}

	dir := filepath.Dir(r.filePath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}

	return os.WriteFile(r.filePath, data, 0644)
}

func (r *LocalConfigReader) GetString(key string) (string, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	switch key {
	case port.ConfigLLMModel:
		return r.config.LLM.Model, nil
	case port.ConfigScreeningSearchTimeout:
		return r.config.Screening.SearchTimeout, nil
	case port.ConfigDeliveryTTL:
		return r.config.Delivery.TTL, nil
	case port.ConfigDeliveryCleanupInterval:
		return r.config.Delivery.CleanupInterval, nil
	default:
		return "", fmt.Errorf("unknown config key: %s", key)
	}
}

func (r *LocalConfigReader) GetFloat(key string) (float64, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	switch key {
	case port.ConfigLLMTemperature:
		return r.config.LLM.Temperature, nil
	case port.ConfigScreeningTrustThreshold:
		return r.config.Screening.TrustThreshold, nil
	case port.ConfigScreeningMatchThreshold:
		return r.config.Screening.MatchThreshold, nil
	case port.ConfigScreeningSalaryWeight:
		return r.config.Screening.SalaryWeight, nil
	case port.ConfigScreeningBenefitWeight:
		return r.config.Screening.BenefitWeight, nil
	case port.ConfigScreeningIntroWeight:
		return r.config.Screening.IntroWeight, nil
	case port.ConfigScreeningLocationSameWeight:
		return r.config.Screening.LocationSameWeight, nil
	case port.ConfigScreeningLocationNearWeight:
		return r.config.Screening.LocationNearWeight, nil
	case port.ConfigScreeningVerifyWeight:
		return r.config.Screening.VerifyWeight, nil
	default:
		return 0, fmt.Errorf("unknown config key: %s", key)
	}
}

func (r *LocalConfigReader) GetInt(key string) (int, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	switch key {
	case port.ConfigRewriteMaxLen:
		return r.config.Rewrite.MaxLength, nil
	case port.ConfigDeliveryMaxBatchSize:
		return r.config.Delivery.MaxBatchSize, nil
	default:
		return 0, fmt.Errorf("unknown config key: %s", key)
	}
}

func (r *LocalConfigReader) GetDuration(key string) (time.Duration, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	var val string
	switch key {
	case port.ConfigCacheTTL:
		val = r.config.Cache.TTL
	case port.ConfigJDTimeout:
		val = r.config.JD.CrawlTimeout
	case port.ConfigScreeningSearchTimeout:
		val = r.config.Screening.SearchTimeout
	case port.ConfigDeliveryTTL:
		val = r.config.Delivery.TTL
	case port.ConfigDeliveryCleanupInterval:
		val = r.config.Delivery.CleanupInterval
	default:
		return 0, fmt.Errorf("unknown config key: %s", key)
	}

	return time.ParseDuration(val)
}

func (r *LocalConfigReader) Set(key string, value interface{}) error {
	r.mu.Lock()
	defer r.mu.Unlock()

	switch key {
	case port.ConfigLLMModel:
		r.config.LLM.Model = fmt.Sprintf("%v", value)
	case port.ConfigLLMTemperature:
		if v, ok := value.(float64); ok {
			r.config.LLM.Temperature = v
		} else if v, ok := value.(string); ok {
			if f, err := strconv.ParseFloat(v, 64); err == nil {
				r.config.LLM.Temperature = f
			}
		}
	case port.ConfigRewriteMaxLen:
		if v, ok := value.(int); ok {
			r.config.Rewrite.MaxLength = v
		} else if v, ok := value.(string); ok {
			if i, err := strconv.Atoi(v); err == nil {
				r.config.Rewrite.MaxLength = i
			}
		}
	default:
		return fmt.Errorf("unknown or unsupported config key: %s", key)
	}

	return r.save()
}

// Keys 返回所有支持的配置键
func Keys() []string {
	return []string{
		port.ConfigLLMModel,
		port.ConfigLLMTemperature,
		port.ConfigRewriteMaxLen,
		port.ConfigCacheTTL,
		port.ConfigJDTimeout,
		port.ConfigSectionOrder,
		port.ConfigExportFormats,
		port.ConfigScreeningTrustThreshold,
		port.ConfigScreeningMatchThreshold,
		port.ConfigScreeningSearchTimeout,
		port.ConfigDeliveryTTL,
		port.ConfigDeliveryCleanupInterval,
		port.ConfigDeliveryMaxBatchSize,
	}
}

// FormatValue 格式化配置值用于显示
func FormatValue(key string, cfg *ConfigModel) string {
	switch key {
	case port.ConfigLLMModel:
		return cfg.LLM.Model
	case port.ConfigLLMTemperature:
		return strconv.FormatFloat(cfg.LLM.Temperature, 'f', 1, 64)
	case port.ConfigRewriteMaxLen:
		return strconv.Itoa(cfg.Rewrite.MaxLength)
	case port.ConfigCacheTTL:
		return cfg.Cache.TTL
	case port.ConfigJDTimeout:
		return cfg.JD.CrawlTimeout
	case port.ConfigSectionOrder:
		return strings.Join(cfg.Rewrite.SectionOrder, ", ")
	case port.ConfigExportFormats:
		return strings.Join(cfg.Export.Formats, ", ")
	case port.ConfigScreeningTrustThreshold:
		return strconv.FormatFloat(cfg.Screening.TrustThreshold, 'f', 1, 64)
	case port.ConfigScreeningMatchThreshold:
		return strconv.FormatFloat(cfg.Screening.MatchThreshold, 'f', 1, 64)
	case port.ConfigScreeningSearchTimeout:
		return cfg.Screening.SearchTimeout
	case port.ConfigDeliveryTTL:
		return cfg.Delivery.TTL
	case port.ConfigDeliveryCleanupInterval:
		return cfg.Delivery.CleanupInterval
	case port.ConfigDeliveryMaxBatchSize:
		return strconv.Itoa(cfg.Delivery.MaxBatchSize)
	}
	return ""
}
