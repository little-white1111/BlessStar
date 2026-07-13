package llm

import (
	"context"
	"fmt"
	"time"

	"resumedesign/internal/port"
)

const (
	claudeBaseURL   = "https://api.anthropic.com"
	claudeAPIVersion = "2023-06-01"
	claudeModel     = "claude-3-sonnet-20240229"
)

// ClaudeClient Anthropic Claude 适配器
type ClaudeClient struct {
	cfg *LLMConfig
}

func NewClaudeClient(apiKey string) *ClaudeClient {
	return &ClaudeClient{
		cfg: &LLMConfig{
			APIKey:      apiKey,
			BaseURL:     claudeBaseURL,
			Model:       claudeModel,
			Temperature: 0.7,
			MaxTokens:   4096,
			Timeout:     120 * time.Second,
		},
	}
}

func (c *ClaudeClient) Name() string {
	return "claude:" + c.cfg.Model
}

// claudeRequest Claude 请求格式
type claudeRequest struct {
	Model       string    `json:"model"`
	MaxTokens   int       `json:"max_tokens"`
	System      string    `json:"system,omitempty"`
	Messages    []claudeMessage `json:"messages"`
	Temperature float64   `json:"temperature"`
}

type claudeMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

// claudeResponse Claude 响应格式
type claudeResponse struct {
	Content []struct {
		Text string `json:"text"`
	} `json:"content"`
	Usage struct {
		InputTokens  int `json:"input_tokens"`
		OutputTokens int `json:"output_tokens"`
	} `json:"usage"`
}

func (c *ClaudeClient) RewriteResume(ctx context.Context, req port.RewriteRequest) (*port.RewriteResponse, error) {
	if req.Config.Model != "" {
		c.cfg.Model = req.Config.Model
	}
	if req.Config.Temperature > 0 {
		c.cfg.Temperature = req.Config.Temperature
	}

	systemPrompt := "你是一个专业的简历优化助手。请根据以下原始简历和岗位要求，对简历进行适当改写。" +
		"要求：\n" +
		"1. 保留简历的核心信息（项目经历、成就奖项、专业技能方向）\n" +
		"2. 适当调整措辞和描述方式以贴合岗位要求\n" +
		"3. 不要伪造或添加不存在的经历\n" +
		"4. 保持原简历的段落结构不变\n" +
		"5. 输出格式与原简历段落结构一致"

	userPrompt := req.OriginalResume + "\n\n" + req.JobDescription

	creq := claudeRequest{
		Model:     c.cfg.Model,
		MaxTokens: c.cfg.MaxTokens,
		System:    systemPrompt,
		Messages: []claudeMessage{
			{Role: "user", Content: userPrompt},
		},
		Temperature: c.cfg.Temperature,
	}

	result, err := c.callClaude(ctx, creq)
	if err != nil {
		return nil, fmt.Errorf("Claude rewrite failed: %w", err)
	}

	if len(result.Content) == 0 {
		return nil, fmt.Errorf("Claude returned empty content")
	}

	totalTokens := result.Usage.InputTokens + result.Usage.OutputTokens

	return &port.RewriteResponse{
		RewrittenContent: result.Content[0].Text,
		ModelUsed:        c.cfg.Model,
		TotalTokens:      totalTokens,
	}, nil
}

func (c *ClaudeClient) callClaude(ctx context.Context, req claudeRequest) (*claudeResponse, error) {
	// Claude API 调用（简化实现，实际需要通过 Anthropic SDK 或自定义 HTTP 请求）
	// 此处复用通用 HTTP 调用模式
	messages := []ChatMessage{
		{Role: "user", Content: req.Messages[0].Content},
	}

	chatReq := ChatRequest{
		Model:       req.Model,
		Messages:    messages,
		Temperature: req.Temperature,
		MaxTokens:   req.MaxTokens,
	}

	c.cfg.BaseURL = claudeBaseURL

	// 使用通用 callLLM（实际上 OpenAI 格式，Claude 需要转换）
	// 生产环境应使用 Anthropic 官方 SDK 或正确格式的 HTTP 请求
	result, err := callLLM(ctx, c.cfg, chatReq.Messages)
	if err != nil {
		return nil, err
	}

	if len(result.Choices) == 0 {
		return nil, fmt.Errorf("Claude returned empty choices")
	}

	return &claudeResponse{
		Content: []struct {
			Text string `json:"text"`
		}{
			{Text: result.Choices[0].Message.Content},
		},
		Usage: struct {
			InputTokens  int `json:"input_tokens"`
			OutputTokens int `json:"output_tokens"`
		}{
			InputTokens:  0,
			OutputTokens: result.Usage.TotalTokens,
		},
	}, nil
}
