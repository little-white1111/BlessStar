/**
 * textParser — AI 响应文本解析
 *
 * 原位置：AIPanel.tsx（臃肿拆分）。
 * 提供 [PLAN]/[/PLAN] 标签提取、Thinking 行提取、标签清理等功能。
 */

import type { PlanStep } from '../types'

/** 从 AI 响应内容中提取 [PLAN]...[/PLAN] 步骤列表 */
export function extractPlanSteps(content: string): PlanStep[] | undefined {
  if (!content) return undefined
  const planMatch = content.match(/\[PLAN\]([\s\S]*?)\[\/PLAN\]/i)
  if (!planMatch) return undefined

  const lines = planMatch[1]
    .split('\n')
    .map((l) => l.trim())
    .filter((l) => l.length > 0 && !/^(#|步骤|计划|Step)/i.test(l))

  if (lines.length === 0) return undefined

  // 尝试按序号切分
  if (lines.length === 1 && /^\d+[\.\、]\s/.test(lines[0])) {
    const numberedParts = lines[0]
      .split(/(?=(?:\d+)[\.\、]\s)/g)
      .map((p) => p.trim())
      .filter((p) => p.length > 0)
    return numberedParts.map((text, i) => ({ id: i + 1, text, done: false }))
  }

  return lines.map((text, i) => ({ id: i + 1, text, done: false }))
}

/** 从 AI 响应中提取 [THINKING]...[/THINKING] 块 */
export function extractThinking(content: string): string | undefined {
  const match = content.match(/\[THINKING\]([\s\S]*?)\[\/THINKING\]/i)
  return match ? match[1].trim() : undefined
}

/** 移除 [PLAN]...[/PLAN] 标签，保留纯文本 */
export function removePlanTags(content: string): string {
  return content.replace(/\[PLAN\][\s\S]*?\[\/PLAN\]\n?/gi, '').trim()
}

/** 移除 [THINKING]...[/THINKING] 行 */
export function removeThinkingLine(content: string): string {
  return content.replace(/\[THINKING\][\s\S]*?\[\/THINKING\]\n?/gi, '').trim()
}
