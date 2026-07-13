package service

import (
	"fmt"
	"os"
	"strings"
	"time"

	"resumedesign/internal/domain"
	"resumedesign/internal/port"
	"resumedesign/internal/repository"

	"github.com/google/uuid"
)

// ResumeService 简历解析用例
type ResumeService struct {
	resumeRepo  *repository.ResumeRepo
	sectionRepo *repository.SectionRepo
	parser      port.ResumeParser
}

func NewResumeService(resumeRepo *repository.ResumeRepo, sectionRepo *repository.SectionRepo, parser port.ResumeParser) *ResumeService {
	return &ResumeService{
		resumeRepo:  resumeRepo,
		sectionRepo: sectionRepo,
		parser:      parser,
	}
}

// UploadAndParse 上传并解析简历
// 不变量：原始简历创建后不可修改（immutable template）
func (s *ResumeService) UploadAndParse(filePath string, importFormat string, title string) (*domain.Resume, error) {
	// 验证文件格式
	validFormats := map[string]bool{"pdf": true, "docx": true, "txt": true}
	if !validFormats[importFormat] {
		return nil, fmt.Errorf("unsupported format: %s", importFormat)
	}

	// 验证文件存在
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		return nil, fmt.Errorf("file not found: %s", filePath)
	}

	// 解析简历（通过 Port 接口，不变量 T2：不依赖具体 adapter 实现）
	parsedSections, err := s.parser.Parse(filePath, importFormat)
	if err != nil {
		return nil, fmt.Errorf("parse resume failed: %w", err)
	}

	// 读取原始内容
	rawData, err := os.ReadFile(filePath)
	if err != nil {
		return nil, fmt.Errorf("read file failed: %w", err)
	}
	rawContent := string(rawData)

	if title == "" {
		title = fmt.Sprintf("简历_%s", time.Now().Format("2006-01-02"))
	}

	now := time.Now()
	resume := &domain.Resume{
		ID:           uuid.New().String(),
		Title:        title,
		RawContent:   rawContent,
		ImportFormat: importFormat,
		CreatedAt:    now,
		UpdatedAt:    now,
	}

	// 持久化
	if err := s.resumeRepo.Create(resume); err != nil {
		return nil, fmt.Errorf("save resume failed: %w", err)
	}

	// 保存段落
	sections := make([]*domain.Section, len(parsedSections))
	for i, ps := range parsedSections {
		sections[i] = &domain.Section{
			ID:      uuid.New().String(),
			ResumeID: resume.ID,
			Type:    ps.Type,
			Title:   ps.Title,
			Content: ps.Content,
			Order:   ps.Order,
		}
	}
	if err := s.sectionRepo.BatchCreate(sections); err != nil {
		return nil, fmt.Errorf("save sections failed: %w", err)
	}

	resume.Sections = sections
	return resume, nil
}

// List 获取简历列表
func (s *ResumeService) List() ([]*domain.Resume, error) {
	return s.resumeRepo.FindAll()
}

// GetByID 获取简历详情（含段落）
func (s *ResumeService) GetByID(id string) (*domain.Resume, error) {
	resume, err := s.resumeRepo.FindByID(id)
	if err != nil {
		return nil, fmt.Errorf("resume not found: %s", id)
	}

	sections, err := s.sectionRepo.FindByResumeID(id)
	if err != nil {
		return nil, err
	}
	resume.Sections = sections
	return resume, nil
}

// Delete 删除简历
func (s *ResumeService) Delete(id string) error {
	// 级联删除 sections（数据库外键约束 CASCADE）
	return s.resumeRepo.Delete(id)
}

// ParseFileContent 直接解析文本内容（从 CLI 粘贴模式）
func (s *ResumeService) ParseFileContent(content string, title string) (*domain.Resume, error) {
	// 写入临时文件
	tmpFile, err := os.CreateTemp("", "resumedesign-txt-*.txt")
	if err != nil {
		return nil, fmt.Errorf("create temp file failed: %w", err)
	}
	defer os.Remove(tmpFile.Name())

	if _, err := tmpFile.WriteString(content); err != nil {
		tmpFile.Close()
		return nil, fmt.Errorf("write temp file failed: %w", err)
	}
	tmpFile.Close()

	return s.UploadAndParse(tmpFile.Name(), "txt", title)
}

// 临时目录路径（不变量 T5）
func getTempDir() string {
	dir := os.TempDir()
	tmpDir := strings.TrimSuffix(dir, string(os.PathSeparator)) + string(os.PathSeparator) + "resumedesign"
	os.MkdirAll(tmpDir, 0755)
	return tmpDir
}
