package service

import (
	"context"
	"crypto/sha256"
	"fmt"
	"strings"
	"time"

	"resumedesign/internal/domain"
	"resumedesign/internal/port"
	"resumedesign/internal/repository"

	"github.com/google/uuid"
)

// RewriteService 智能改写用例
type RewriteService struct {
	sessionRepo *repository.SessionRepo
	diffRepo    *repository.DiffRepo
	resumeRepo  *repository.ResumeRepo
	sectionRepo *repository.SectionRepo
	jdRepo      *repository.JDRepo
	llmClient   port.LLMClient
	cache       port.Cache
}

func NewRewriteService(
	sessionRepo *repository.SessionRepo,
	diffRepo *repository.DiffRepo,
	resumeRepo *repository.ResumeRepo,
	sectionRepo *repository.SectionRepo,
	jdRepo *repository.JDRepo,
	llmClient port.LLMClient,
	cache port.Cache,
) *RewriteService {
	return &RewriteService{
		sessionRepo: sessionRepo,
		diffRepo:    diffRepo,
		resumeRepo:  resumeRepo,
		sectionRepo: sectionRepo,
		jdRepo:      jdRepo,
		llmClient:   llmClient,
		cache:       cache,
	}
}

// CreateSession 创建改写会话
// 不变量 4：同一份原始简历可对应多个 JD 产生多个独立改写版本
func (s *RewriteService) CreateSession(resumeID string, jdID string, config domain.RewriteConfig) (*domain.RewriteSession, error) {
	// 验证简历和 JD 存在
	if _, err := s.resumeRepo.FindByID(resumeID); err != nil {
		return nil, fmt.Errorf("resume not found: %s", resumeID)
	}
	if _, err := s.jdRepo.FindByID(jdID); err != nil {
		return nil, fmt.Errorf("JD not found: %s", jdID)
	}

	session := &domain.RewriteSession{
		ID:          uuid.New().String(),
		ResumeID:    resumeID,
		JdID:        jdID,
		Status:      domain.SessionDraft,
		Model:       config.Model,
		Temperature: config.Temperature,
		CreatedAt:   time.Now(),
	}

	// 不变量 T4：状态变更必须持久化到数据库
	if err := s.sessionRepo.Create(session); err != nil {
		return nil, fmt.Errorf("create session failed: %w", err)
	}

	return session, nil
}

// RunRewrite 执行改写
// 不变量 T1：LLM 调用必须经过 port.LLMClient 接口
// 不变量 5：每次 LLM 调用必须记录 model、temperature、prompt 摘要
func (s *RewriteService) RunRewrite(ctx context.Context, sessionID string) (*domain.RewriteSession, error) {
	session, err := s.sessionRepo.FindByID(sessionID)
	if err != nil {
		return nil, fmt.Errorf("session not found: %s", sessionID)
	}

	// 不变量 1：原始简历作为不可变模板
	resume, err := s.resumeRepo.FindByID(session.ResumeID)
	if err != nil {
		return nil, err
	}

	// 加载简历段落
	sections, err := s.sectionRepo.FindByResumeID(resume.ID)
	if err != nil {
		return nil, err
	}
	resume.Sections = sections

	jd, err := s.jdRepo.FindByID(session.JdID)
	if err != nil {
		return nil, err
	}

	// 状态变更 → 持久化（不变量 T4）
	session.Status = domain.SessionProcessing
	s.sessionRepo.Update(session)

	// 从缓存读取原始简历
	cacheKey := "resume:" + resume.ID
	originalContent, err := s.cache.Get(cacheKey)
	if err != nil {
		// 缓存未命中，从数据库加载
		originalContent = resume.RawContent
		// 写入缓存
		_ = s.cache.Set(cacheKey, originalContent, 24*time.Hour)
	}

	// 构建 LLM 请求
	sectionInfos := make([]port.SectionInfo, len(sections))
	for i, sec := range sections {
		sectionInfos[i] = port.SectionInfo{
			Type:  string(sec.Type),
			Title: sec.Title,
			Order: sec.Order,
		}
	}

	llmReq := port.RewriteRequest{
		OriginalResume: resume.AsPrompt(),
		JobDescription: jd.AsPrompt(),
		Sections:       sectionInfos,
		Config: port.RewriteConfig{
			Model:       session.Model,
			Temperature: session.Temperature,
		},
	}

	// 不变量 T1：通过 port.LLMClient 调用
	result, err := s.llmClient.RewriteResume(ctx, llmReq)
	if err != nil {
		session.Status = domain.SessionFailed
		session.ErrorMessage = err.Error()
		session.CompletedAt = timePtr(time.Now())
		s.sessionRepo.Update(session)
		return session, fmt.Errorf("rewrite failed: %w", err)
	}

	// 记录审计信息（不变量 5）
	session.Model = result.ModelUsed
	session.RewrittenContent = result.RewrittenContent
	session.PromptHash = promptHash(resume.RawContent + jd.RawContent)
	session.Status = domain.SessionCompleted
	now := time.Now()
	session.CompletedAt = &now
	s.sessionRepo.Update(session)

	// 计算差异（不变量 6：必须包含差异标注）
	diffs := computeDiffs(resume, session)
	if err := s.diffRepo.BatchCreate(diffs); err != nil {
		return nil, fmt.Errorf("save diffs failed: %w", err)
	}
	session.Diff = diffs

	return session, nil
}

// ListSessions 获取改写会话列表
func (s *RewriteService) ListSessions() ([]*domain.RewriteSession, error) {
	return s.sessionRepo.FindAll()
}

// GetSession 获取会话详情（含 diffs）
func (s *RewriteService) GetSession(id string) (*domain.RewriteSession, error) {
	session, err := s.sessionRepo.FindByID(id)
	if err != nil {
		return nil, fmt.Errorf("session not found: %s", id)
	}

	diffs, err := s.diffRepo.FindBySessionID(id)
	if err != nil {
		return nil, err
	}
	session.Diff = diffs
	return session, nil
}

// computeDiffs 计算原始简历与改写结果的逐段差异
// 不变量 6：改写结果必须包含差异标注
func computeDiffs(original *domain.Resume, session *domain.RewriteSession) []*domain.Diff {
	var diffs []*domain.Diff
	rewrittenContent := session.RewrittenContent

	// 按段落对比
	for _, sec := range original.Sections {
		diff := &domain.Diff{
			ID:              uuid.New().String(),
			SessionID:       session.ID,
			SectionType:     sec.Type,
			OriginalContent: sec.Content,
			SortOrder:       sec.Order,
			Confidence:      0.9,
		}

		// 简单判断改写内容是否包含原始内容
		originalTrimmed := strings.TrimSpace(sec.Content)
		if originalTrimmed == "" {
			diff.ChangeType = domain.ChangeUnchanged
			diff.RewrittenContent = ""
		} else if strings.Contains(rewrittenContent, originalTrimmed) {
			diff.ChangeType = domain.ChangeUnchanged
			diff.RewrittenContent = sec.Content
		} else {
			diff.ChangeType = domain.ChangeModified
			diff.RewrittenContent = "[改写后的内容，请在导出文件中查看]"
		}

		diffs = append(diffs, diff)
	}

	return diffs
}

func promptHash(input string) string {
	h := sha256.Sum256([]byte(input))
	return fmt.Sprintf("%x", h[:16])
}

func timePtr(t time.Time) *time.Time {
	return &t
}
