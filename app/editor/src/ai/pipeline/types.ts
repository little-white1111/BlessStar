/**
 * pipeline/types — 管线上下文共享类型
 *
 * 定义在 pipeline/ 子目录内各 stage 之间传递的共享上下文。
 * 由 stage-router → stage-intent → pipelineManager → stage-execute → stage-render 按序填充。
 */

import type { CompactIndex } from '../context-manager/contextBuilder'
import type { SkillMatch } from '../context-manager/skillRouter'
import type { RoutingContext } from '../context-manager/thinkLevelSelector'
import type { BlessStarIntent } from '../intent/trie_dict'
import type { QueryResult } from '../context-manager/adaptiveIndex'
import type { FunctionToolParam, ToolCall, PlanStep } from '../types'

/** L0 采集 hint（匹配 collectL0Hint 返回类型） */
export interface L0Hint {
  operationHint: string
  subjectHint: string
}

/** Think Level 选择结果（匹配 selectThinkLevel 返回类型） */
export interface ThinkLevelResult {
  level: string
  suggestedTemperature: number
}

// ── 采集与注入 ────────────────────────────────────────────────────────

export interface CollectedHints {
  l0: L0Hint | null
  l1: Array<{ label: string; aiHint: string; configKey: string }> | null
}

// ── 理解Agent 输出（专题七：兼容新旧格式）─────────────────────────────

export interface UnderstandingAgentOutput {
  todo: Array<{
    subject: string
    /** 意图类别（默认与旧版 operation 兼容） */
    intent: string
    value: string | null
    condition: string | null
    /** 本条是否为纯概念咨询（走咨询Agent 而非工具执行） */
    is_chat: boolean
    /** 专题七新字段：匹配的配置键（必须在检索 Top-5 候选中） */
    target_config_key?: string | null
    /** 专题七新字段：修改目标值 */
    new_value?: string | null
    /** 专题七新字段：是否歧义（有多个类似候选） */
    is_ambiguous?: boolean
    /** 专题七新字段：是否危险操作（删除/重置等） */
    is_dangerous?: boolean
  }>
}

// ── P1: sessionState（请求级状态保持）───────────────────────────

/**
 * P1: 请求级状态保持。在 ASK 回路/自定义回路中复用，
 * 避免重跑管线时的冷启动重复计算。
 * 每次冷启动仍可正常工作，sessionState 只起加速复用作用。
 */
export interface SessionState {
  /** L1 匹配缓存：subject → configKey */
  subjectToKey: Record<string, string>
  /** 激活的 domain shard 名称列表 */
  activeDomains: string[]
  /** schema 字段类型表（复用，避免重复 fetch） */
  schemaTypeMap: Array<[string, string]>
  /** 重试计数器（上限 3，达上限后强制冷启动） */
  attemptCount: number
  /** D38-8-方案3：工具摘要记录，保留最近 2 轮 */
  toolSummaries: string[]
}

// ── PipelineContext ───────────────────────────────────────────────────

/**
 * 管线上下文：各 stage 按序填充，最终汇集到 pipelineManager 中。
 *
 * Stage Router  → 填充 routing 域
 * Stage Intent  → 填充 intent 域 + hints
 * Manager Core  → 填充 plan + 调用 UA/降级 LLM
 * Stage Execute → 填充 execution 域
 * Stage Render  → 消费全部后清理
 */
export interface PipelineContext {
  // ── 原始输入 ──
  userInput: string

  // ── ① Stage Router ──
  isCommand: boolean
  skillMatch: SkillMatch
  l0Hint: L0Hint | null
  isMultiClause: boolean
  clauses: string[]

  // ── ②③④⑤⑩a Stage Intent ──
  compressed: BlessStarIntent | null
  routingCtx: RoutingContext
  thinkLevel: ThinkLevelResult
  effectiveIndex: CompactIndex | null
  adaptiveResults: QueryResult[]
  fieldScores: Array<{ field: string; score: number }>
  toolMatch: { tools: string[] }
  toolDefs: FunctionToolParam[]
  cPathFields: string[]
  isChatQuery: boolean
  hints: CollectedHints
  uaUserMessage: string

  // ── ⑩b⑩c⑪ Manager Core（填充）─
  uaSuccess: boolean
  isUA: boolean

  // ── ⑩c 映射层产物（由 mapTripletsToToolCalls 或降级路径填充）─
  toolCallsToExecute: ToolCall[]
  planStepToolRanges: number[][]

  // ── ⑪ 降级路径产物 ──
  cleanContent: string

  // ── ⑩b 咨询Agent 回复（延迟到沙箱后展示）─
  chatAnswer: string | null

  // ── D38-4-INV-04: ASK 管线挂起 ─
  awaitingConfirmation: boolean
  suspendedState: {
    question: string
    /** 候选列表，每项含 label（中文名）、configKey、aiHint（自然语言描述） */
    candidates: Array<{ label: string; configKey: string; aiHint: string }>
    originalSubject: string
    /** 用户原始输入，选择候选后重新执行管线 */
    originalUserInput: string
    fallbackMessage: string
    intent: string
    subject: string
    value: string | null
    planSteps: import('../types').PlanStep[]
  } | null

  // ── P1: 请求级状态保持 ─
  sessionState: SessionState | null
}

/**
 * 创建空的 PipelineContext（仅含 userInput）。
 * 各 stage 在对应字段上按序填充。
 */
export function createPipelineContext(userInput: string): PipelineContext {
  return {
    userInput,
    isCommand: false,
    skillMatch: { matched: false },
    l0Hint: null,
    isMultiClause: false,
    clauses: [],
    compressed: null,
    routingCtx: { userInput, skillRouterEnabled: true, metaModeEnabled: false },
    thinkLevel: { level: 'auto', suggestedTemperature: 0.3 },
    effectiveIndex: null,
    adaptiveResults: [],
    fieldScores: [],
    toolMatch: { tools: [] },
    toolDefs: [],
    cPathFields: [],
    isChatQuery: false,
    hints: { l0: null, l1: null },
    uaUserMessage: '',
    uaSuccess: false,
    isUA: false,
    toolCallsToExecute: [],
    planStepToolRanges: [],
    cleanContent: '',
    chatAnswer: null,
    awaitingConfirmation: false,
    suspendedState: null,
    sessionState: null,
  }
}
