import type { AIBridgeConfig, AICompletionRequest, AICompletionResponse } from './types'

/**
 * CloudProvider: 通用 OpenAI 兼容接口（DeepSeek / OpenAI）。
 * 通过 IPC 调用主进程 fetch，避免渲染进程直接跨域请求。
 */
class CloudProvider implements AIBridge {
  private baseUrl: string
  private apiKey: string
  private model: string
  private embeddingModel: string

  constructor(config: AIBridgeConfig) {
    // DeepSeek 默认 https://api.deepseek.com，OpenAI 默认 https://api.openai.com
    this.baseUrl = config.baseUrl || (
      config.provider === 'deepseek'
        ? 'https://api.deepseek.com'
        : 'https://api.openai.com'
    )
    this.apiKey = config.apiKey || ''
    this.model = config.model || (
      config.provider === 'deepseek' ? 'deepseek-chat' : 'gpt-4o'
    )
    // EMB: embedding 模型名，默认同 chat 模型
    this.embeddingModel = config.embeddingModel || this.model
  }

  async complete(req: AICompletionRequest): Promise<AICompletionResponse> {
    const body: Record<string, unknown> = {
      model: req.model || this.model,
      messages: req.messages.map((m) => ({
        role: m.role === 'tool' ? 'tool' : m.role,
        content: m.content,
        ...(m.tool_call_id ? { tool_call_id: m.tool_call_id } : {}),
      })),
      stream: false,
    }

    if (req.tools && req.tools.length > 0) {
      body.tools = req.tools.map((t) => ({
        type: 'function',
        function: {
          name: t.name,
          description: t.description,
          parameters: t.parameters,
        },
      }))
    }

    const raw = await window.blessstar.aiChat({
      baseUrl: this.baseUrl,
      apiKey: this.apiKey,
      model: this.model,
      body: JSON.stringify(body),
    })
    const data = JSON.parse(raw)
    const msg = data.choices?.[0]?.message || data.message || {}

    const response: AICompletionResponse = {
      message: {
        role: msg.role || 'assistant',
        content: msg.content || '',
      },
    }

    if (msg.tool_calls && msg.tool_calls.length > 0) {
      response.tool_calls = msg.tool_calls.map((tc: { id?: string; type?: string; function?: { name: string; arguments: string } }) => ({
        id: tc.id || `call_${Date.now()}`,
        type: 'function',
        function: {
          name: tc.function?.name || '',
          arguments: tc.function?.arguments || '{}',
        },
      }))
    }

    if (data.usage) {
      response.usage = {
        prompt_tokens: Number(data.usage.prompt_tokens) || 0,
        completion_tokens: Number(data.usage.completion_tokens) || 0,
        total_tokens: Number(data.usage.total_tokens) || 0,
      }
    }

    return response
  }

  /** EMB: 调用 OpenAI 兼容 embedding API */
  async embed(text: string): Promise<number[]> {
    const body = JSON.stringify({
      model: this.embeddingModel,
      input: text,
    })
    const raw = await window.blessstar.aiEmbed({
      url: `${this.baseUrl.replace(/\/+$/, '')}/v1/embeddings`,
      apiKey: this.apiKey,
      body,
    })
    const data = JSON.parse(raw)
    return data.data?.[0]?.embedding || []
  }
}

/**
 * OllamaProvider: 本地 Ollama 推理。
 * 通过 IPC 调用主进程 fetch 到 http://localhost:11434/api/chat。
 */
class OllamaProvider implements AIBridge {
  private model: string
  private embeddingModel: string
  private ollamaUrl: string

  constructor(config: AIBridgeConfig) {
    this.model = config.ollamaModel || 'qwen2.5-coder:7b'
    // EMB: embedding 模型名，默认同 chat 模型
    this.embeddingModel = config.embeddingModel || this.model
    this.ollamaUrl = (config.ollamaUrl || 'http://localhost:11434').replace(/\/+$/, '')
  }

  async complete(req: AICompletionRequest): Promise<AICompletionResponse> {
    const body: Record<string, unknown> = {
      model: req.model || this.model,
      messages: req.messages.map((m) => ({
        role: m.role === 'tool' ? 'tool' : m.role,
        content: m.content,
        ...(m.tool_call_id ? { tool_call_id: m.tool_call_id } : {}),
      })),
      stream: false,
    }

    if (req.tools && req.tools.length > 0) {
      body.tools = req.tools.map((t) => ({
        type: 'function',
        function: {
          name: t.name,
          description: t.description,
          parameters: t.parameters,
        },
      }))
    }

    const raw = await window.blessstar.aiComplete(JSON.stringify(body))
    const data = JSON.parse(raw)
    const msg = data.message || {}

    const response: AICompletionResponse = {
      message: {
        role: msg.role || 'assistant',
        content: msg.content || '',
      },
    }

    if (msg.tool_calls && msg.tool_calls.length > 0) {
      response.tool_calls = msg.tool_calls.map((tc: { id?: string; type?: string; function?: { name: string; arguments: string } }) => ({
        id: tc.id || `call_${Date.now()}`,
        type: 'function',
        function: {
          name: tc.function?.name || '',
          arguments: tc.function?.arguments || '{}',
        },
      }))
    }

    // Ollama /api/chat 返回: prompt_eval_count, eval_count, prompt_eval_duration, eval_duration
    const promptTokens = Number(data.prompt_eval_count) || 0
    const completionTokens = Number(data.eval_count) || 0
    if (promptTokens || completionTokens) {
      response.usage = {
        prompt_tokens: promptTokens,
        completion_tokens: completionTokens,
        total_tokens: promptTokens + completionTokens,
      }
    }

    return response
  }

  /** EMB: 调用 Ollama /api/embeddings */
  async embed(text: string): Promise<number[]> {
    const body = JSON.stringify({
      model: this.embeddingModel,
      prompt: text,
    })
    const raw = await window.blessstar.aiEmbed({
      url: `${this.ollamaUrl}/api/embeddings`,
      body,
    })
    const data = JSON.parse(raw)
    return data.embedding || []
  }
}

export function createAIBridge(config: AIBridgeConfig): AIBridge {
  switch (config.provider) {
    case 'openai':
    case 'deepseek':
      return new CloudProvider(config)
    case 'ollama':
      return new OllamaProvider(config)
    default:
      return new OllamaProvider(config)
  }
}

export interface AIBridge {
  complete(req: AICompletionRequest): Promise<AICompletionResponse>
  /** EMB: 将文本转为 embedding 向量 */
  embed(text: string): Promise<number[]>
}
