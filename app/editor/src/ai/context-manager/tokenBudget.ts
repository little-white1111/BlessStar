/**
 * tokenBudget — Token 预算预检 + 超限降级
 *
 * 估算 context 的 token 消耗，在超限时触发降级策略：
 * - 策略 1: 截断 compact 索引（优先保留 field_semantics）
 * - 策略 2: 移除 constraint_knowledge
 * - 策略 3: 全量检索降级为仅用 B 路径（移除 C 路径）
 */

import type { CompactIndex } from './contextBuilder'

// ── 粗略估算常数 ─────────────────────────────────────────────────────
// 按 ~4 chars / token 估算（英文为主），中文约 ~2 chars / token
const CHARS_PER_TOKEN_BASE = 4
const CHARS_PER_TOKEN_CJK  = 2

// 各层预算（单位：tokens）
export const BUDGET_LAYER1 = 500   // 工作记忆：system prompt + 用户输入
export const BUDGET_LAYER2 = 2048  // 业务知识库：compact 索引
export const BUDGET_LAYER3 = 2048  // 工具契约：tool defs
export const BUDGET_TOTAL  = 4096  // 总预算

// ── 估算 ─────────────────────────────────────────────────────────────

export interface TokenEstimate {
  /** 预估 token 数 */
  tokens: number
  /** 是否超出总预算 */
  overBudget: boolean
  /** 超出量（如果超限） */
  excessTokens: number
}

/**
 * 粗略估算字符串的 token 数。
 * 中文字符按 CJK 系数，其他按 base 系数。
 */
export function estimateTokens(text: string): number {
  let cjkChars = 0
  let otherChars = 0
  for (const ch of text) {
    if (ch.match(/[\u4e00-\u9fff\u3400-\u4dbf\uf900-\ufaff]/)) {
      cjkChars++
    } else {
      otherChars++
    }
  }
  return Math.ceil(cjkChars / CHARS_PER_TOKEN_CJK + otherChars / CHARS_PER_TOKEN_BASE)
}

/**
 * 估算 Context 总 token 数。
 */
export function estimateContextTokens(
  systemPrompt: string,
  userInput: string,
  compactIndex: CompactIndex | null,
  toolDelta: string | null,
): TokenEstimate {
  let total = 0

  // Layer 1: system prompt + user input
  total += estimateTokens(systemPrompt)
  total += estimateTokens(userInput)

  // Layer 2: compact 索引
  if (compactIndex) {
    if (compactIndex.fieldSemantics) total += estimateTokens(compactIndex.fieldSemantics)
    if (compactIndex.domainKnowledge) total += estimateTokens(compactIndex.domainKnowledge)
    if (compactIndex.constraintKnowledge) total += estimateTokens(compactIndex.constraintKnowledge)
  }

  // tool delta
  if (toolDelta) {
    total += estimateTokens(toolDelta)
  }

  const overBudget = total > BUDGET_TOTAL
  return {
    tokens: total,
    overBudget,
    excessTokens: overBudget ? total - BUDGET_TOTAL : 0,
  }
}

// ── 降级策略 ─────────────────────────────────────────────────────────

export interface DegradationResult {
  /** 降级后的 compact 索引 */
  compactIndex: CompactIndex | null
  /** 是否禁用了 C 路径（LLM 字段选择器） */
  disableCPath: boolean
  /** 降级原因 */
  reason: string
}

/**
 * 预检 token 预算，超限时自动降级。
 *
 * 降级优先级（从低破坏性到高破坏性）：
 * 1. 移除 constraint_knowledge
 * 2. 截断 field_semantics（仅保留前 N 行）
 * 3. 移除 domain_knowledge
 * 4. 禁用 C 路径（LLM 字段选择器）
 */
export function degradeIfOverBudget(
  systemPrompt: string,
  userInput: string,
  compactIndex: CompactIndex | null,
  toolDelta: string | null,
): DegradationResult {
  if (!compactIndex) {
    return { compactIndex: null, disableCPath: false, reason: '无 compact 索引' }
  }

  let estimate = estimateContextTokens(systemPrompt, userInput, compactIndex, toolDelta)
  if (!estimate.overBudget) {
    return { compactIndex, disableCPath: false, reason: '预算充足，无需降级' }
  }

  let degraded = { ...compactIndex }
  const steps: string[] = []

  // 降级 1: 移除 constraint_knowledge
  if (degraded.constraintKnowledge) {
    degraded.constraintKnowledge = ''
    estimate = estimateContextTokens(systemPrompt, userInput, degraded, toolDelta)
    steps.push('移除 constraint_knowledge')
    if (!estimate.overBudget) {
      return { compactIndex: degraded, disableCPath: false, reason: steps.join(' → ') }
    }
  }

  // 降级 2: 截断 field_semantics（保留前 20 行）
  if (degraded.fieldSemantics) {
    const lines = degraded.fieldSemantics.split('\n')
    if (lines.length > 20) {
      degraded.fieldSemantics = lines.slice(0, 20).join('\n') + '\n... (truncated)'
      estimate = estimateContextTokens(systemPrompt, userInput, degraded, toolDelta)
      steps.push('截断 field_semantics 至 20 行')
      if (!estimate.overBudget) {
        return { compactIndex: degraded, disableCPath: false, reason: steps.join(' → ') }
      }
    }
  }

  // 降级 3: 移除 domain_knowledge
  if (degraded.domainKnowledge) {
    degraded.domainKnowledge = ''
    estimate = estimateContextTokens(systemPrompt, userInput, degraded, toolDelta)
    steps.push('移除 domain_knowledge')
    if (!estimate.overBudget) {
      return { compactIndex: degraded, disableCPath: false, reason: steps.join(' → ') }
    }
  }

  // 降级 4: 禁用 C 路径
  steps.push('禁用 C 路径（LLM 字段选择器）')
  return { compactIndex: degraded, disableCPath: true, reason: steps.join(' → ') }
}
