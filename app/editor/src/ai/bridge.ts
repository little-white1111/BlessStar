import type {
  AIBridgeConfig, AICompletionRequest, AICompletionResponse,
  AIMessage, ToolCall, FunctionToolParam,
} from './types'

// === IPC-backed AI Tool Functions for MVP ===

const IPC_TOOLS: Record<string, (...args: unknown[]) => Promise<string>> = {
  create_schema_field: async (args: unknown) => {
    const result = await window.blessstar.executeTool('create_schema_field', args)
    return JSON.stringify(result)
  },
  update_gate_rule: async (args: unknown) => {
    const result = await window.blessstar.executeTool('update_gate_rule', args)
    return JSON.stringify(result)
  },
  validate_config: async (args: unknown) => {
    const a = args as Record<string, string>
    const result = await window.blessstar.validateConfig(a.configJson || '{}')
    return JSON.stringify(result)
  },
  suggest_field_type: async (args: unknown) => {
    const result = await window.blessstar.executeTool('suggest_field_type', args)
    return JSON.stringify(result)
  },
  generate_normalizer_template: async (args: unknown) => {
    const result = await window.blessstar.executeTool('generate_normalizer_template', args)
    return JSON.stringify(result)
  },
}

async function mockCompletion(messages: AIMessage[], tools?: FunctionToolParam[]): Promise<AICompletionResponse> {
  const lastMsg = messages[messages.length - 1]
  if (!lastMsg) {
    return {
      message: { role: 'assistant', content: '您好，我是 AI 助手。请问需要什么帮助？' },
    }
  }

  // If the last message is a tool result, generate a follow-up
  if (lastMsg.role === 'tool') {
    return {
      message: {
        role: 'assistant',
        content: `已处理工具调用。结果：\n\`\`\`json\n${lastMsg.content}\n\`\`\`\n\n如需继续操作，请告知。`,
      },
    }
  }

  // If tools are available and user asks for an action, simulate a tool call
  if (tools && tools.length > 0 && lastMsg.role === 'user') {
    const userText = lastMsg.content.toLowerCase()
    const matchedTool = tools.find((t) => userText.includes(t.name))
    if (matchedTool && IPC_TOOLS[matchedTool.name]) {
      // Parse args from user message (very basic)
      const args: Record<string, string> = {}
      const argNames = Object.keys(matchedTool.parameters.properties as Record<string, unknown>)
      for (const argName of argNames) {
        const regex = new RegExp(`${argName}[=:]\\s*["']?([^"'\\s,，。]+)`, 'i')
        const match = userText.match(regex)
        if (match) args[argName] = match[1]
      }

      // Execute via IPC immediately and return result
      const result = await IPC_TOOLS[matchedTool.name](args)

      const toolCalls: ToolCall[] = [{
        id: `call_mock_${Date.now()}`,
        type: 'function',
        function: {
          name: matchedTool.name,
          arguments: JSON.stringify(args),
        },
      }]

      return {
        message: {
          role: 'assistant',
          content: `我将为您执行 \`${matchedTool.name}\` 工具。\n结果：\n\`\`\`json\n${result}\n\`\`\``,
        },
        tool_calls: toolCalls,
      }
    }
  }

  return {
    message: {
      role: 'assistant',
      content: '收到您的消息。请描述具体操作，或从以下工具中选择：\n' +
        (tools || []).map((t) => `- \`${t.name}\`: ${t.description}`).join('\n'),
    },
  }
}

class OpenAIProvider implements AIBridge {
  async complete(req: AICompletionRequest): Promise<AICompletionResponse> {
    return mockCompletion(req.messages, req.tools)
  }
}

/**
 * OllamaProvider: 接收调用方传人的上下文（contextBuilder 产出或已有的 messages），
 * 不做历史累积，直接透传给后端。符合 CTX-01（每轮只发 Layer 1+2+3，不做对话历史累积）。
 */
class OllamaProvider implements AIBridge {
  private baseUrl: string
  private model: string

  constructor(config: AIBridgeConfig) {
    this.baseUrl = config.ollamaUrl || 'http://localhost:11434'
    this.model = config.ollamaModel || 'qwen2.5-coder:7b'
  }

  async complete(req: AICompletionRequest): Promise<AICompletionResponse> {
    const body: Record<string, unknown> = {
      model: this.model,
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

    return response
  }
}

export function createAIBridge(config: AIBridgeConfig): AIBridge {
  switch (config.provider) {
    case 'openai':
      return new OpenAIProvider()
    case 'ollama':
      return new OllamaProvider(config)
    default:
      return new OpenAIProvider()
  }
}

export interface AIBridge {
  complete(req: AICompletionRequest): Promise<AICompletionResponse>
}
