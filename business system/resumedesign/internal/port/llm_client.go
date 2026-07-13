package port

import "context"

// LLMClient 大模型客户端 Port 接口
// 不变量 T1：所有 LLM 调用必须经过此接口，禁止在 service/domain 中直接发起 HTTP 请求
// 实现：adapter/llm/openai.go（GPT），adapter/llm/claude.go（Claude）
type LLMClient interface {
	// RewriteResume 改写简历
	RewriteResume(ctx context.Context, req RewriteRequest) (*RewriteResponse, error)

	// Name 返回模型名称（用于审计日志）
	Name() string
}

// RewriteRequest 改写请求
type RewriteRequest struct {
	OriginalResume string        // 原始简历全文
	JobDescription string        // JD 全文
	Sections       []SectionInfo // 原简历段落结构（用于保留段落顺序约束）
	Config         RewriteConfig // 改写参数
}

// SectionInfo 段落结构信息
type SectionInfo struct {
	Type  string
	Title string
	Order int
}

// RewriteConfig 改写配置
type RewriteConfig struct {
	Model       string  // 模型名称
	Temperature float64 // 0.0 ~ 2.0
	MaxLength   int     // 最大 Token 数
}

// RewriteResponse 改写响应
type RewriteResponse struct {
	RewrittenContent string // 改写后的简历全文
	ModelUsed        string // 实际使用的模型
	TotalTokens      int    // 消耗的 Token 数（用于计费/审计）
}
