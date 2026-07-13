package service

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"resumedesign/internal/domain"
	"resumedesign/internal/port"
	"resumedesign/internal/repository"
)

// ExportService 导出用例
type ExportService struct {
	sessionRepo *repository.SessionRepo
	resumeRepo  *repository.ResumeRepo
	exporter    port.Exporter
}

func NewExportService(sessionRepo *repository.SessionRepo, resumeRepo *repository.ResumeRepo, exporter port.Exporter) *ExportService {
	return &ExportService{
		sessionRepo: sessionRepo,
		resumeRepo:  resumeRepo,
		exporter:    exporter,
	}
}

// Export 导出改写后简历
// 不变量 T5：临时文件通过临时目录管理
// 不变量 1：原始简历永不修改（只导出改写结果）
func (s *ExportService) Export(sessionID string, format string) (string, error) {
	session, err := s.sessionRepo.FindByID(sessionID)
	if err != nil {
		return "", fmt.Errorf("session not found: %s", sessionID)
	}

	if session.Status != domain.SessionCompleted {
		return "", fmt.Errorf("session %s is not completed (status: %s)", sessionID, session.Status)
	}

	if session.RewrittenContent == "" {
		return "", fmt.Errorf("session %s has no rewritten content", sessionID)
	}

	resume, err := s.resumeRepo.FindByID(session.ResumeID)
	if err != nil {
		return "", err
	}

	// 不变量 T5：临时文件通过临时目录管理
	tmpDir := filepath.Join(os.TempDir(), "resumedesign")
	os.MkdirAll(tmpDir, 0755)

	outputPath := filepath.Join(tmpDir, fmt.Sprintf("%s_rewritten_%s.%s",
		sanitizeFilename(resume.Title),
		time.Now().Format("20060102_150405"),
		format))

	// 通过 Port 接口导出（不变量 T2）
	if err := s.exporter.Export(session.RewrittenContent, format, outputPath); err != nil {
		return "", fmt.Errorf("export failed: %w", err)
	}

	return outputPath, nil
}

func sanitizeFilename(name string) string {
	// 移除文件名中的非法字符
	cleaned := make([]byte, 0, len(name))
	for _, c := range []byte(name) {
		if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' {
			cleaned = append(cleaned, c)
		} else {
			cleaned = append(cleaned, '_')
		}
	}
	result := string(cleaned)
	if result == "" {
		result = "resume"
	}
	return result
}
