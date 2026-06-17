// === AI System Shared Types ===

export type AIProviderType = 'openai' | 'ollama'

export interface AIMessage {
  role: 'system' | 'user' | 'assistant' | 'tool'
  content: string
  tool_call_id?: string
  name?: string
}

export interface FunctionToolParam {
  name: string
  description: string
  parameters: Record<string, unknown>
}

export interface ToolCall {
  id: string
  type: 'function'
  function: {
    name: string
    arguments: string
  }
}

export interface AICompletionRequest {
  messages: AIMessage[]
  tools?: FunctionToolParam[]
  temperature?: number
  max_tokens?: number
}

export interface AICompletionResponse {
  message: AIMessage
  tool_calls?: ToolCall[]
}

export interface ToolResult {
  success: boolean
  data?: unknown
  error?: string
}

export interface FunctionTool {
  definition: FunctionToolParam
  execute(args: Record<string, unknown>): Promise<ToolResult>
}

export interface AIBridgeConfig {
  provider: AIProviderType
  apiKey?: string
  baseUrl?: string
  model?: string
  ollamaUrl?: string
  ollamaModel?: string
}

// Validation result from BlessStar Schema/Gate
export interface ValidationResult {
  valid: boolean
  errors: ValidationError[]
}

export interface ValidationError {
  path: string
  message: string
  code?: string
}

export const MAX_TOOL_RETRIES = 3
