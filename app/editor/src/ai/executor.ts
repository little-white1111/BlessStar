import type { ToolResult, ToolCall, FunctionTool } from './types'
import { MAX_TOOL_RETRIES } from './types'
import { FUNCTION_TOOLS } from './tools'
import { evaluatePreGates, TOOL_PRE_GATE_RULES, deepEvaluate } from './preGate'

/** Find a FunctionTool by name */
export function findTool(name: string): FunctionTool | undefined {
  return FUNCTION_TOOLS.find((t) => t.definition.name === name)
}

/**
 * Execute a single tool call with two-stage Pre-Gate validation.
 * Stage 1: JS quick filter (not_empty/regex) — synchronous.
 * Stage 2: C deep evaluation (bs_gate_evaluator_evaluate) — async via IPC.
 *
 * DAY38-06: 二段式校验（JS 快筛 + C 精判）
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

  // Stage 1: JS 快筛
  const preGateError = evaluatePreGates(TOOL_PRE_GATE_RULES[tc.function.name], args)
  if (preGateError !== null) {
    return { success: false, error: `[Pre-Gate JS] ${preGateError}` }
  }

  // Stage 2: C 深判
  const deepError = await deepEvaluate(tc.function.name, args)
  if (deepError !== null) {
    return { success: false, error: deepError }
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

    if (attempt < MAX_TOOL_RETRIES) {
      continue
    }
  }

  return { result: lastResult, attempts: MAX_TOOL_RETRIES }
}
