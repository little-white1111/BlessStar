/**
 * pipeline/stage-router — ① L0 采集（精简版）
 *
 * Stage 1：入口路由决策（仅 L0 信息采集）。
 *
 * 专题六修正（PIPELINE-10）：① L0 为信息采集器，不跳过管线。
 * /command 命中后产出 operationHint + subjectHint，注入理解Agent，
 * 不再走 executeSkillWorkflow 直接返回。
 *
 * 专题七优化（D38-OPT-STAGE12）：⑧ 多子句检测 + ⑨ is_chat 检测
 * 已从 executeStageRouter 分离为独立导出函数，
 * RAG 路径不再执行（由理解Agent per-item 原生分流），
 * 仅在降级路径按需调用。
 */

import { matchSkill, collectL0Hint, parseCommand } from '../context-manager/skillRouter'
import { splitUserIntent } from '../parallelExecutor'
import type { PipelineContext } from './types'

/**
 * 执行 Stage Router（精简版）：仅 L0 采集。
 *
 * 不修改 ctx 以外的任何状态，纯上下文填充。
 */
export function executeStageRouter(ctx: PipelineContext): void {
  const text = ctx.userInput

  // ── ① L0: 采集 operationHint + subjectHint ──
  const skillMatch = matchSkill(text)
  ctx.isCommand = skillMatch.matched && !!skillMatch.route
  ctx.skillMatch = skillMatch
  ctx.l0Hint = collectL0Hint(text)

  // ── ⑧ 多子句检测：填充 ctx.clauses 供概念层逐子句检查使用 ──
  const { clauses } = detectMultiClause(text)
  ctx.clauses = clauses

  // ⑨ is_chat 已移至降级路径（见 detectChatQuery）
}

/**
 * ⑧ 多子句检测（降级路径用）。
 *
 * RAG 路径中多意图由理解Agent per-item is_chat 原生分流，
 * 不需提前切分；降级路径无理解Agent，需此作为前置检测。
 */
export function detectMultiClause(text: string): {
  clauses: string[]
  isMultiClause: boolean
} {
  const clauses = splitUserIntent(text)
  return { clauses, isMultiClause: clauses.length > 1 }
}

/**
 * ⑨ is_chat 检测（降级路径用）。
 *
 * RAG 路径中 chat 分流由理解Agent per-item is_chat 承担；
 * 降级路径无 LLM 分流能力，仅靠 skill route + 简单正则。
 */
export function detectChatQuery(
  text: string,
  skillMatch: ReturnType<typeof matchSkill>,
): boolean {
  // 兼容旧版 SKILL_ROUTES 和 UNIFIED_SKILLS（parseCommand）
  const isCommand = skillMatch.matched || parseCommand(text).matched
  return isCommand
    ? /什么是|是什么|解释|为什么|怎么样/.test(text)
    : false
}
