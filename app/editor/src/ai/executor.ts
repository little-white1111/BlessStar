import type { ToolResult, ToolCall, FunctionTool } from './types'
import { MAX_TOOL_RETRIES } from './types'
import { FUNCTION_TOOLS } from './tools'

/** Find a FunctionTool by name */
export function findTool(name: string): FunctionTool | undefined {
  return FUNCTION_TOOLS.find((t) => t.definition.name === name)
}

/**
 * Execute a single tool call with BlessStar validation gateway.
 * Returns error to AI for retry on validation failure.
 */
export async function executeToolCall(tc: ToolCall): Promise<ToolResult> {
  const tool = findTool(tc.function.name)
  if (!tool) {
    return { success: false, error: `未知工具: ${tc.function.name}` }
  }

  let args: Record<string, unknown>
  try {
    args = JSON.parse(tc.function.arguments)
  } catch {
    return { success: false, error: `工具参数 JSON 解析失败: ${tc.function.arguments}` }
  }

  return tool.execute(args)
}

/**
 * Execute a tool call with retry up to MAX_TOOL_RETRIES.
 * This implements AI-03: Tool execution calls BlessStar C ABI validation.
 * On failure, returns error for AI to retry.
 */
export async function executeWithRetry(tc: ToolCall): Promise<{ result: ToolResult; attempts: number }> {
  let lastResult: ToolResult = { success: false, error: '未执行' }

  for (let attempt = 1; attempt <= MAX_TOOL_RETRIES; attempt++) {
    lastResult = await executeToolCall(tc)

    if (lastResult.success) {
      return { result: lastResult, attempts: attempt }
    }

    // If not last attempt, we'd retry — but since the tool execution
    // is deterministic for MVP (no side effects), we just return.
    // In production, AI would modify args and retry.
    if (attempt < MAX_TOOL_RETRIES) {
      // Allow next iteration to re-execute
      continue
    }
  }

  return { result: lastResult, attempts: MAX_TOOL_RETRIES }
}
