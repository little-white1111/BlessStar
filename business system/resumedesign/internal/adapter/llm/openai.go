package llm

import (
	"context"
	"fmt"
	"time"

	"resumedesign/internal/port"
)

const (
	openAIBaseURL = "https://api.openai.com"
	defaultModel  = "gpt-4o"
)

// OpenAIClient OpenAI GPT 适配器
type OpenAIClient struct {
	cfg *LLMConfig
}

func NewOpenAIClient(apiKey string) *OpenAIClient {
	return &OpenAIClient{
		cfg: &LLMConfig{
			APIKey:      apiKey,
			BaseURL:     openAIBaseURL,
			Model:       defaultModel,
			Temperature: 0.7,
			MaxTokens:   4096,
			Timeout:     120 * time.Second,
		},
	}
}

func (c *OpenAIClient) Name() string {
	return "openai:" + c.cfg.Model
}

func (c *OpenAIClient) RewriteResume(ctx context.Context, req port.RewriteRequest) (*port.RewriteResponse, error) {
	// 更新配置
	if req.Config.Model != "" {
		c.cfg.Model = req.Config.Model
	}
	if req.Config.Temperature > 0 {
		c.cfg.Temperature = req.Config.Temperature
	}

	// 构建 prompt
	systemPrompt := "你是一个专业的简历优化助手。请根据以下原始简历和岗位要求，对简历进行适当改写。" +
		"要求：\n" +
		"1. 保留简历的核心信息（项目经历、成就奖项、专业技能方向）\n" +
		"2. 适当调整措辞和描述方式以贴合岗位要求\n" +
		"3. 不要伪造或添加不存在的经历\n" +
		"4. 保持原简历的段落结构不变\n" +
		"5. 输出格式与原简历段落结构一致"

	userPrompt := req.OriginalResume + "\n\n" + req.JobDescription

	messages := []ChatMessage{
		{Role: "system", Content: systemPrompt},
		{Role: "user", Content: userPrompt},
	}

	result, err := callLLM(ctx, c.cfg, messages)
	if err != nil {
		return nil, fmt.Errorf("OpenAI rewrite failed: %w", err)
	}

	if len(result.Choices) == 0 {
		return nil, fmt.Errorf("OpenAI returned empty choices")
	}

	return &port.RewriteResponse{
		RewrittenContent: result.Choices[0].Message.Content,
		ModelUsed:        c.cfg.Model,
		TotalTokens:      result.Usage.TotalTokens,
	}, nil
}
