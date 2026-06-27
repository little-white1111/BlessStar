/**
 * pipeline/stage-execute — ⑫ 工具执行 + Pre-Gate + Trace + ⑬⑭ 证据链
 *
 * Stage 4：工具执行与证据链验证。
 *
 * ⑫a Pre-Gate 校验
 * ⑫b 执行工具调用
 * ⑫c ExecutionTrace + ToolCallRegistry 记录
 * ⑬ roundVerify 证据配平
 * ⑭ checkFabricationRisk 编造检测
 *
 * 专题六：⑩c 映射层 value/condition → tool args 自动填充（G4 修复）
 */

import { executeToolCall } from '../executor'
import { executionTrace, toolCallRegistry, roundVerify, checkFabricationRisk } from '../context-manager/executionTrace'
import { buildToolDelta } from '../context-manager/toolDeltaFormatter'
import { GATE_FACTORIES } from '../GateFactoryBridge'
import { resultLines, toolLabel } from '../formatters/toolFormatter'
import { operationToTools } from '../operationMapper'
import type { ToolCall, ToolResult, PlanStep } from '../types'
import type { ToolCard } from '../components/SandboxTodo'

// ── 执行结果 ──────────────────────────────────────────────────────────

export interface ExecutionResult {
  toolResults: ToolResult[]
  toolCards: ToolCard[]
  allToolSuccess: boolean
  toolDelta: ReturnType<typeof buildToolDelta> | undefined
  verifyResult: ReturnType<typeof roundVerify>
  fabricationWarning: string | null
}

/**
 * 执行工具调用链（⑫a~⑫d）+ 证据链验证（⑬）+ 编造检测（⑭）。
 *
 * @returns ExecutionResult 供 stage-render 消费
 */
export async function executeStage(
  toolCallsToExecute: ToolCall[],
  planSteps: PlanStep[],
  planStepToolRanges: number[][],
  isUA: boolean,
  cleanContent: string,
): Promise<ExecutionResult> {
  const toolResults: ToolResult[] = []
  const toolCards: ToolCard[] = []
  let allToolSuccess = true

  // ── ⑫a~⑫d 逐工具执行 ──
  for (const toolCall of toolCallsToExecute) {
    // ⑫a Pre-Gate 校验 + Gate 工厂层识别（实际 Pre-Gate 由 executor.ts 内部用真实 args 执行）
    const factoryLayer = GATE_FACTORIES.find(f => f.name === toolCall.function.name)?.layer

    const result = await executeToolCall(toolCall)  // eslint-disable-line no-await-in-loop
    toolResults.push(result)

    // ExecutionTrace + ToolCallRegistry
    const outputSummary = result.success
      ? `✅ ${toolLabel(toolCall.function.name)} 成功`
      : `❌ ${toolLabel(toolCall.function.name)}: ${result.error || '失败'}`
    executionTrace.newRound()
    const node = executionTrace.addNode({
      toolName: toolCall.function.name,
      input: {},
      outputSummary,
      dependsOn: [],
    })
    toolCallRegistry.record(
      node.callId,
      toolCall.function.name,
      outputSummary,
      result.success ? 'success' : 'failed',
    )

    // ToolCard 构建
    const lines = resultLines(toolCall.function.name, result)
    toolCards.push({
      callId: node.callId,
      toolName: toolCall.function.name,
      args: {},
      success: result.success,
      outputLines: factoryLayer ? [`[${factoryLayer}]`, ...lines] : lines,
      preGatePassed: true,
    })

    if (!result.success) allToolSuccess = false
  }

  // Tool delta
  const latestResult = toolResults[toolResults.length - 1]
  const latestCall = toolCallsToExecute[toolCallsToExecute.length - 1]
  const toolDelta = latestCall && latestResult
    ? buildToolDelta(latestCall.function.name, latestResult)
    : undefined

  // ── ⑬ 证据链验证 ──
  if (planSteps && planSteps.length > 0) {
    if (isUA) {
      // UA 路径：用 planStepToolRanges + toolCards.callId 配平
      for (let i = 0; i < planSteps.length; i++) {
        const indices = planStepToolRanges[i]
        if (indices && indices.length > 0) {
          planSteps[i].done = true
          const lastIdx = indices[indices.length - 1]
          planSteps[i].callId = toolCards[lastIdx]?.callId
          // 多 tool 步骤：记录所有 tool 的 callId 供 roundVerify 全量校验
          planSteps[i].allCallIds = indices.map(idx => toolCards[idx]?.callId).filter(Boolean) as string[]
        }
      }
    } else {
      // 降级路径：toolCallsToExecute[i].id 与 node.callId 一致
      for (let i = 0; i < planSteps.length; i++) {
        if (i < toolCallsToExecute.length) {
          planSteps[i].done = true
          planSteps[i].callId = toolCallsToExecute[i].id
        }
      }
    }
  }
  const verifyResult = roundVerify(planSteps, cleanContent)

  // ── ⑭ 编造风险检测 ──
  const fabricationWarning = checkFabricationRisk(
    cleanContent,
    planSteps,
    toolCallsToExecute.length,
  )

  return { toolResults, toolCards, allToolSuccess, toolDelta, verifyResult, fabricationWarning }
}

// ── ActionItemMeta ────────────────────────────────────────────────────

/** P3: ActionItem 显式元数据，替代字符串推断 */
export interface ActionItemMeta {
  /** L1 解析的 2 级 key 前缀（如 "livedesign.live2d"），用于 list_configs prefix */
  domainPrefix?: string
  /** 字段类型（'file' | 'string' | 'number' | 'boolean'），用于路由决策 */
  configType?: string
  /** 路径字段是否为空 */
  isPathEmpty?: boolean
}

/**
 * ⑩c⑩d 映射层：理解Agent 三元组 → ToolCall 数组（G4 修复版：value/condition 自动填充）
 *
 * 专题六修正（G4）：⑩c 映射层从 todo[].value / todo[].condition 自动构造 tool args JSON，
 * 不再硬编码 '{}'。
 * P3 修复：改用 meta 显式字段替代 value.includes('.') 字符串推断。
 */
export function mapTripletsToToolCalls(
  items: Array<{ subject: string; operation: string; value: string | null; condition: string | null; meta?: ActionItemMeta }>,
  subjectToKey?: Record<string, string>,
): { toolCallsToExecute: ToolCall[]; planStepToolRanges: number[][] } {
  const toolCallsToExecute: ToolCall[] = []
  const planStepToolRanges: number[][] = []

  for (const item of items) {
    const rangeStart = toolCallsToExecute.length

    const toolNames = operationToTools(item.operation)

    for (const tn of toolNames) {
      // G4 修复：从 value/condition 构造 tool args
      // G7 修复：从 subject → configKey 注入 key 参数
      const toolArgs: Record<string, unknown> = {}
      if (item.value !== null) toolArgs.value = item.value
      if (item.condition !== null) toolArgs.condition = item.condition
      // 注入 key（工具需要时使用，如 read/write_config_value）
      const resolvedKey = subjectToKey?.[item.subject]
      if (resolvedKey) toolArgs.key = resolvedKey
      // P3 修复：使用 meta.domainPrefix 替代 value.includes('.') 推断
      if (tn === 'list_configs' && item.meta?.domainPrefix) {
        toolArgs.prefix = item.meta.domainPrefix
        delete toolArgs.value
      }
      // P3 修复：使用 meta.configType 替代 !value.includes('.') 推断
      if (tn === 'list_directory' && item.meta?.configType === 'file') {
        if (item.value) toolArgs.path = item.value
        delete toolArgs.value
      }

      toolCallsToExecute.push({
        id: `call_ua_${item.operation}_${Date.now()}_${Math.random().toString(36).slice(2, 6)}`,
        type: 'function',
        function: { name: tn, arguments: JSON.stringify(toolArgs) },
      })
    }

    planStepToolRanges.push(
      Array.from({ length: toolCallsToExecute.length - rangeStart }, (_, i) => rangeStart + i),
    )
  }

  return { toolCallsToExecute, planStepToolRanges }
}
