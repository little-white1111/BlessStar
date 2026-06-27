/**
 * pipeline/stage-render — ⑮ 展示 + ⑯ feedback + ⑰ wrap-up
 *
 * Stage 5：结果展示、反馈记录、总结汇报。
 *
 * ⑮ 工具结果格式化展示（UA 路径按 planStep 分组 / 降级路径 T1 paradigm）
 * ⑯ FeedbackCollector 记录
 * ⑰ Wrap-up 总结（UA 路径模板合成 / 降级路径 LLM 汇报）
 */

import { paradigmRegistry } from '../context-manager/paradigm'
import { BusinessAdapterRegistry } from '../business-adapter/registry'
import { toolLabel, resultLines } from '../formatters/toolFormatter'
import type { FeedbackCollector } from '../context-manager/feedbackCollector'
import type { ToolCall, ToolResult, PlanStep, AIMessage } from '../types'

/** getMessageSetter / getSuggestionSetter: 回调接口（宿主组件注入） */

export interface RenderCallbacks {
  appendMessage: (msg: AIMessage) => void
  setSuggestion: (data: string) => void
  addFeedback: (
    collector: FeedbackCollector,
    intent: string,
    success: boolean,
    type: 'translation_incorrect' | 'tool_wrong',
  ) => void
}

export interface WrapUpFn {
  (text: string, allToolSuccess: boolean, planSteps: PlanStep[]): Promise<void>
}

/** ⑮ 格式化工具结果展示内容 */
export function formatToolResultContent(
  isUA: boolean,
  planSteps: PlanStep[],
  planStepToolRanges: number[][],
  toolCallsToExecute: ToolCall[],
  toolResults: ToolResult[],
): string {
  const allLines: string[] = []

  if (isUA) {
    // UA 路径：按 planStep 分组展示（一个 planStep 可能对应多个 tool）
    for (let ps = 0; ps < planSteps.length; ps++) {
      const indices = planStepToolRanges[ps] || []
      if (indices.length === 0) continue
      const allOk = indices.every(idx => toolResults[idx]?.success)
      const stepLabel = planSteps[ps].text.replace(/^\[.*?\]\s*/, '')
      const toolLabels = indices.map(idx => toolLabel(toolCallsToExecute[idx]?.function.name)).join(' → ')
      allLines.push(allOk ? `✅ ${stepLabel}: ${toolLabels} 成功` : `❌ ${stepLabel}: ${toolLabels} 失败`)
      // 附加每个 tool 的执行结果详情
      for (const idx of indices) {
        const tc = toolCallsToExecute[idx]
        const r = toolResults[idx]
        if (tc && r) {
          const lines = resultLines(tc.function.name, r)
          allLines.push(...lines.map(l => `  ${l}`))
        }
      }
    }
  } else {
    // 降级路径：T1 paradigm 翻译展示
    for (let i = 0; i < toolCallsToExecute.length; i++) {
      const tc = toolCallsToExecute[i]
      const r = toolResults[i]
      const identity = BusinessAdapterRegistry.getSystemPromptIdentity()
      const template = paradigmRegistry.getTemplate(identity, tc.function.name)
      const prefix = template?.success?.match(/^([^：{]+)/)?.[1] || toolLabel(tc.function.name)
      const lines = resultLines(tc.function.name, r)
      allLines.push(`🔧 ${prefix}:`)
      allLines.push(...lines.map(l => `  ${l}`))
    }
  }

  return allLines.join('\n')
}

/** ⑯ FeedbackCollector 记录 */
export function recordFeedback(
  collector: FeedbackCollector,
  toolCallsToExecute: ToolCall[],
  allToolSuccess: boolean,
): void {
  if (toolCallsToExecute.length > 0) {
    collector.record({
      intent: toolCallsToExecute[0].function.name,
      originalOutput: allToolSuccess ? '工具执行成功' : '工具执行失败',
      userCorrection: '',
      type: allToolSuccess ? 'translation_incorrect' : 'tool_wrong',
    })
  }
}
