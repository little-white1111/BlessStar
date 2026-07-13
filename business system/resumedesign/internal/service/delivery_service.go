package service

import (
	"context"
	"fmt"
	"path/filepath"
	"time"

	"resumedesign/internal/domain"
	"resumedesign/internal/port"
	"resumedesign/internal/repository"

	"github.com/google/uuid"
)

// DeliveryService 批量投递编排服务
type DeliveryService struct {
	deliveryRepo *repository.DeliveryRepo
	resumeRepo   *repository.ResumeRepo
	scoringRepo  *repository.ScoringRepo
	exporter     port.Exporter
	fileStorage  port.FileStorage
	browser      port.BrowserOpener
	config       port.ConfigReader
}

func NewDeliveryService(
	deliveryRepo *repository.DeliveryRepo,
	resumeRepo *repository.ResumeRepo,
	scoringRepo *repository.ScoringRepo,
	exporter port.Exporter,
	fileStorage port.FileStorage,
	browser port.BrowserOpener,
	config port.ConfigReader,
) *DeliveryService {
	return &DeliveryService{
		deliveryRepo: deliveryRepo,
		resumeRepo:   resumeRepo,
		scoringRepo:  scoringRepo,
		exporter:     exporter,
		fileStorage:  fileStorage,
		browser:      browser,
		config:       config,
	}
}

// StartDelivery 启动批量投递
// 不变量 D5：TTL 在创建时锁定
// 不变量 D4：单份简历单次投递
func (s *DeliveryService) StartDelivery(
	ctx context.Context,
	resumeID string,
	targets []DeliveryTargetInput,
	exportFormat string,
) (*domain.DeliverySession, error) {
	if len(targets) == 0 {
		return nil, fmt.Errorf("at least one target required")
	}
	if len(targets) > 50 {
		return nil, fmt.Errorf("max batch size is 50, got %d", len(targets))
	}

	// 读取简历
	resume, err := s.resumeRepo.FindByID(resumeID)
	if err != nil {
		return nil, fmt.Errorf("resume not found: %s", resumeID)
	}

	// 读取 TTL 配置（默认 7 天）
	ttl := 7 * 24 * time.Hour
	if s.config != nil {
		if d, err := s.config.GetDuration("delivery.ttl"); err == nil && d > 0 {
			ttl = d
		}
	}

	// D5：TTL 在创建时锁定
	session := domain.NewDeliverySession(resumeID, resume.Title, ttl)
	session.ID = uuid.New().String()

	// 创建投递目标
	for _, t := range targets {
		target := domain.NewDeliveryTarget(
			session.ID, t.ScoredJDID,
			t.Company, t.Position, t.URL,
			session.ExpireAt,
		)
		target.ID = uuid.New().String()

		// 导出简历
		outputContent := resume.RawContent
		filename := fmt.Sprintf("%s_%s.%s",
			sanitizeFilename(t.Company),
			sanitizeFilename(t.Position),
			exportFormat)
		relativePath := filepath.Join(session.ID, target.ID, filename)
		tmpPath := s.fileStorage.FullPath(relativePath)

		if err := s.exporter.Export(outputContent, exportFormat, tmpPath); err != nil {
			target.MarkFailed()
			s.deliveryRepo.SaveTarget(target)
			continue
		}

		target.MarkExported(exportFormat, relativePath)
		session.AddTarget(target)
		s.deliveryRepo.SaveTarget(target)
	}

	// 持久化投递会话
	if err := s.deliveryRepo.CreateSession(session); err != nil {
		return nil, fmt.Errorf("create delivery session failed: %w", err)
	}

	// 打开浏览器
	for _, target := range session.Targets {
		if target.URL != "" && target.Status != domain.TargetFailed {
			s.browser.Open(target.URL)
		}
	}

	session.SetCompleted()
	s.deliveryRepo.UpdateSession(session)

	return session, nil
}

// ListDeliveries 获取投递历史
func (s *DeliveryService) ListDeliveries() ([]*domain.DeliverySession, error) {
	return s.deliveryRepo.FindAllSessions()
}

// GetDelivery 获取投递详情
func (s *DeliveryService) GetDelivery(id string) (*domain.DeliverySession, error) {
	return s.deliveryRepo.FindSessionByID(id)
}

// RevokeDelivery 撤销投递
// 不变量 D2：删除过期记录
// 不变量 D3：文件与记录共存亡
func (s *DeliveryService) RevokeDelivery(id string) error {
	session, err := s.deliveryRepo.FindSessionByID(id)
	if err != nil {
		return fmt.Errorf("delivery not found: %s", id)
	}

	// 删除简历文件（D3：删记录必删文件）
	for _, target := range session.Targets {
		if target.ExportedFilePath != "" {
			s.fileStorage.Delete(target.ExportedFilePath)
		}
	}

	// 删除投递记录（D3：文件与记录共存亡）
	return s.deliveryRepo.DeleteSession(id)
}
