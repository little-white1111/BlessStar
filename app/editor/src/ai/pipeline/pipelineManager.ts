/**
 * pipeline/pipelineManager — AI 管线核心编排引擎
 *
 * 专题七修订版 20 步闭环管线的主入口。
 * 由 AIPanel.tsx 的 handleSend 调用，组装 5 个 Stage：
 *
 *   Stage 1 (stage-router)   → ① L0 采集 + ⑧ 多子句 + ⑨ is_chat
 *   Stage 2 (stage-intent)   → ②③④⑤⑥⑦⑩a 意图+分片+采集
 *   Core     (本文件)         → ⑩b 理解Agent + ⑩c 映射 + ⑪ 降级
 *   Stage 3 (stage-execute)  → ⑫ PreGate+执行+Trace + ⑬ roundVerify + ⑭ fabrication
 *   Stage 4 (stage-render)   → ⑮ 展示 + ⑯ feedback + ⑰ wrap-up
 *
 * 专题六核心修正：
 *   G1: /command 路径不再跳过管线，改为产出 L0 hint 注入理解Agent（PIPELINE-10）
 *   G4: ⑩c 映射层 value/condition → tool args 自动填充
 *   G5: 降级路径 compactIndex 对齐 PIPELINE-12（仅 fieldSemantics）
 */

import type { AIBridge } from '../bridge'
import type { AIMessage, PlanStep } from '../types'
import { buildContext } from '../context-manager/contextBuilder'
import { SYSTEM_PROMPT } from '../prompts/system'
import { REPLY_AGENT_PROMPT } from '../prompts/reply'
import { getConsultationPrompt, CONSULTATION_AGENT_PROMPT } from '../prompts/consultation'
import { BusinessAdapterRegistry } from '../business-adapter/registry'
import { FeedbackCollector } from '../context-manager/feedbackCollector'
import { buildToolDelta } from '../context-manager/toolDeltaFormatter'
import { validateOperationForConfig, SYSTEM_SCOPED_OPS, routeIntentToOperations, routeQueryByValueType, inferSemanticGroup, operationToTools, isIntentCompatibleWithValueType } from '../operationMapper'
import { LABEL_TO_KEY, AI_HINTS, keyToLabel } from '../tools/configLabels'
import { toolResultsText, toolLabel } from '../formatters/toolFormatter'
import { extractPlanSteps, extractThinking, removePlanTags, removeThinkingLine } from './textParser'
import { executeStageRouter, detectMultiClause, detectChatQuery } from './stage-router'
import { executeStageIntent, executeLegacyL1ForFallback, executeIndexShardLoad, parseUnderstandingAgentOutput, correctUAIntents } from './stage-intent'
import { retrievePerClause, getValueTypeFromCandidates } from './retriever'
import { buildUAPromptWithCandidates } from '../prompts/understanding'
import { executeStage, mapTripletsToToolCalls } from './stage-execute'
import { recordFeedback } from './stage-render'
import { createPipelineContext, type PipelineContext } from './types'
import { queryAllLayers, resolveRoute, confirmRoute, seedConceptKeywords } from '../context-manager/adaptiveIndex'
import { findConceptByInput, getAllConcepts } from './bizKnowledge'
import { buildRouteDecision, formatRouteDecision, getAllNaturalLangSkills, skillConfidenceThreshold, parseCommand, resolveParam, type RouteDecision, type DirectExecution } from '../context-manager/skillRouter'

// ── 外部接口（AIPanel.tsx 实现）──────────────────────────────────────

export interface PipelineCallbacks {
  /** 追加消息到对话列表 */
  appendMessage: (msg: AIMessage) => void
  /** 更新最后一条 assistant 消息（如追加 toolCards） */
  updateLastAssistant: (updater: (msg: AIMessage) => AIMessage) => void
  /** 设置当前建议（供采纳）；传入 null 清空 */
  setSuggestion: (data: string | null) => void
  /** 设置处理中状态 */
  setProcessing: (v: boolean) => void
  /** 反馈收集器引用 */
  feedbackRef: { current: FeedbackCollector }
  /** 上一轮 toolDelta 引用 */
  lastToolDeltaRef: { current: ReturnType<typeof buildToolDelta> | undefined }
  /** 获取 AI bridge 引用 */
  getBridge: () => AIBridge
  /** 获取历史消息 */
  getMessages: () => AIMessage[]
  /** 本轮管线的配置写入条目（用于回退按钮，writes + versionIds） */
  onWriteEntries?: (writes: Array<{ key: string; value: string }>, newVersionIds: Record<string, string>) => void
  /** D38-4-INV-04: ASK 管线挂起回调（通知 UI 存储挂起状态） */
  onSuspend?: (state: {
    question: string
    candidates: Array<{ label: string; configKey: string; aiHint: string }>
    originalSubject: string
    originalUserInput: string
    fallbackMessage: string
    intent: string
    subject: string
    value: string | null
    planSteps: PlanStep[]
    /** P1: sessionState 快照，ASK 回路重跑时传入避免冷启动 */
    sessionState?: import('./types').SessionState | null
  }) => void
  /** P1: ASK 回路/自定义回路重跑时传入 sessionState，避免冷启动 */
  sessionState?: import('./types').SessionState | null
}

// ── 理解Agent 结果类型 ───────────────────────────────────────────────

type UAResult =
  | { outcome: 'success'; planSteps: PlanStep[] }
  | { outcome: 'all_chat' }       // 全部项为纯咨询，已由咨询Agent 处理
  | { outcome: 'failed' }         // 解析失败或网络异常，需降级
  | { outcome: 'config_rejected'; reason: string } // CONFIG_OPERATIONS 拒绝
  | { outcome: 'l1_unresolved'; unresolvedItems: string[]; candidates: Array<{ label: string; configKey: string; aiHint: string }>; suggestions: string } // L1 未命中，需回问用户
  | { outcome: 'lookup_unresolved'; candidates: Array<{ label: string; configKey: string; aiHint: string }>; originalSubject: string; intent: string; subject: string; value: string | null } // LOOKUP 三路未匹配→ASK
  | { outcome: 'ambiguous'; question: string; candidates: Array<{ label: string; configKey: string; aiHint: string }>; originalSubject: string; intent: string; subject: string; value: string | null } // 专题七：歧义主动澄清

// ── 主入口 ────────────────────────────────────────────────────────────

/**
 * 执行完整的 AI 管线（20 步闭环）。
 *
 * G1 修正：/command 路径不再提前 return，走完整 UA 链路。
 */
export async function executePipeline(
  text: string,
  callbacks: PipelineCallbacks,
): Promise<void> {
  const ctx = createPipelineContext(text)

  // ── P1: 注入 sessionState（ASK 回路/自定义回路重跑时避免冷启动）────
  if (callbacks.sessionState) {
    ctx.sessionState = callbacks.sessionState
    ctx.sessionState.attemptCount++
    // 防死循环：attemptCount 达上限 3 后强制冷启动
    if (ctx.sessionState.attemptCount >= 3) {
      ctx.sessionState = null
    }
  }

  // ── Stage 1: 路由决策（① L0 采集 + ⑧ 多子句 + ⑨ is_chat）─────
  executeStageRouter(ctx)

  // ── D38-11: 命令门控（最高优先级 — 在概念短路/UA 之前）───────
  // /command 不直接映射 tool，而是翻译为确定意图 + 参数，
  // 复用 BM25 检索 + intent×valueType 兼容性检查，绕过 UA 和 ConceptLayer。
  {
    const parsed = parseCommand(text)
    if (parsed.matched) {
      // ── 命令门控命中 ──

      // 情况 1：无参数 + QUERY_LIST → list_configs() 列出全部
      if (!parsed.rest && parsed.intent === 'QUERY_LIST') {
        const toolCall: import('../types').ToolCall = {
          id: 'cmd_list_all',
          type: 'function',
          function: { name: 'list_configs', arguments: '{}' },
        }
        const planStep: import('../types').PlanStep = {
          id: 0, text: '列出所有配置', done: false,
        }
        const result = await executeStage([toolCall], [planStep], [[0, 0]], false, '')
        const textOutput = toolResultsText([toolCall], result.toolResults)
        callbacks.appendMessage({ role: 'assistant', content: textOutput.trim() || '✅ 已列出所有配置项。' })
        callbacks.setProcessing(false)
        return
      }

      // 情况 2：无参数 + 其他 intent → 提示用法
      if (!parsed.rest) {
        callbacks.appendMessage({
          role: 'assistant',
          content: `请输入要${parsed.description}的配置项名称。\n格式：/${parsed.command} <配置名>`,
        })
        callbacks.setProcessing(false)
        return
      }

      // 情况 3：MODIFY 缺值
      if (parsed.intent === 'MODIFY' && !parsed.value) {
        callbacks.appendMessage({
          role: 'assistant',
          content: `请指定值，格式：/${parsed.command} <配置名> <值>`,
        })
        callbacks.setProcessing(false)
        return
      }

      // 情况 4：有参数 → 两步检索 + intent×valueType 过滤
      const resolved = await resolveParam(parsed.rest)

      if (!resolved.found || resolved.candidates.length === 0) {
        callbacks.appendMessage({
          role: 'assistant',
          content: `「${parsed.rest}」在当前注册表中不存在，如果想要查看当前注册表中注册的所有配置，请输入'/list'`,
        })
        callbacks.setProcessing(false)
        return
      }

      // 按 intent×valueType 过滤兼容候选
      const compatibleCandidates = resolved.candidates.filter(c =>
        isIntentCompatibleWithValueType(parsed.intent, c.valueType),
      )

      if (compatibleCandidates.length === 0) {
        const actionDesc = parsed.intent === 'QUERY_LIST' ? 'LIST' : parsed.intent === 'QUERY_SINGLE' ? 'READ' : '修改'
        callbacks.appendMessage({
          role: 'assistant',
          content: `「${parsed.rest}」不支持 ${actionDesc} 操作`,
        })
        callbacks.setProcessing(false)
        return
      }

      // 有兼容候选：1 个 → 直接执行；≥2 个 → ASK
      if (compatibleCandidates.length === 1) {
        const target = compatibleCandidates[0]
        const opName = routeQueryByValueType(parsed.intent, target.valueType, true)

        // 构建 tool calls
        const toolCalls: import('../types').ToolCall[] = []
        const planSteps: import('../types').PlanStep[] = []

        let stepId = 0
        if (parsed.intent === 'MODIFY' && parsed.value) {
          // MODIFY: READ + WRITE
          toolCalls.push({
            id: `cmd_read_${stepId}`,
            type: 'function',
            function: { name: 'read_config_value', arguments: JSON.stringify({ key: target.configKey }) },
          })
          toolCalls.push({
            id: `cmd_write_${stepId}`,
            type: 'function',
            function: { name: 'write_config_value', arguments: JSON.stringify({ key: target.configKey, value: parsed.value }) },
          })
          planSteps.push({ id: stepId++, text: `读取${target.label}`, done: false })
          planSteps.push({ id: stepId++, text: `设置${target.label}为${parsed.value}`, done: false })
        } else {
          // QUERY_LIST / QUERY_SINGLE
          const toolName = opName === 'BROWSE_DIR' ? 'list_directory' : 'read_config_value'
          const argPayload: Record<string, unknown> = {}
          if (opName === 'BROWSE_DIR') {
            argPayload.path = target.configKey
          } else {
            argPayload.key = target.configKey
          }
          toolCalls.push({
            id: `cmd_${stepId}`,
            type: 'function',
            function: { name: toolName, arguments: JSON.stringify(argPayload) },
          })
          planSteps.push({ id: stepId++, text: `执行${parsed.command} ${target.label}`, done: false })
        }

        const result = await executeStage(toolCalls, planSteps, [[0, toolCalls.length - 1]], false, '')
        const textOutput = toolResultsText(toolCalls, result.toolResults)
        callbacks.appendMessage({ role: 'assistant', content: textOutput.trim() || '✅ 已完成。' })
        callbacks.setProcessing(false)
        return
      }

      // ≥2 兼容候选 → ASK
      const lines = compatibleCandidates.map((c, i) =>
        `${i + 1}，${c.label}`,
      )
      callbacks.appendMessage({
        role: 'assistant',
        content: `「${parsed.rest}」指的是以下哪项？\n${lines.join('\n')}`,
      })
      callbacks.setProcessing(false)
      return
    }
  }

  // ── ⑧ 多子句：不再走 legacy 并行执行器。
  //     （理解Agent 已原生支持多意图 per-item is_chat 分流，
  //       多子句检测仅填充 ctx.clauses 供后续扩展使用）

  // ── 概念/配置智能路由（D38-8-INV-02: 配置层优先）──────────────
  // 在进入 Stage 2 之前，优先通过 AdaptiveIndex 做三层路由：
  //   1. 配置层命中 → 走正常 RAG 路径（不拦截）
  //   2. 概念层 freq≥1 → 概念短路（跳过 UA + LLM）
  //   3. 概念层 freq=0 → 查 bizKnowledge 边界词 → 主动反问
  //   4. 全 miss → 走正常 LLM 路径
  //
  // D38-MULTICLAUSE-FIX: 多子句场景下，全输入可能含某子句命中配置层
  // 导致其他子句的概念层检查被全局跳过。改为逐子句检测：
  //   - 全输入先做概念短路（freq≥1 只可能发生在已学习的全输入匹配）
  //   - freq=0 的概念级 UB 改为逐子句：过滤掉含 MODIFY 关键词的子句，
  //     对纯查询子句独立检查，有则 ASK，无则正常 RAG
  const routeHits = queryAllLayers(text)
  const { configCandidates, conceptHit } = resolveRoute(routeHits)

  // D38-FIX-OVERAGGRESSIVE: 概念短路也检查查询/修改模式，
  // 防止"当前有哪些配置，帮我把房间号改成10041，当前有哪些模型"
  // 这类输入因 boundaryKeyword 局部匹配而触发概念短路
  if (configCandidates.length === 0 && conceptHit && conceptHit.freq >= 1 &&
      !/有哪些|有几项|查看|列出|看下|改成|设为|调整为|当前.*配置|当前.*模型|当前.*设置|当前.*选项/.test(text)) {
    // ── 概念短路（D38-8-INV-03: freq≥1 直接短路）──
    const conceptEntry = findConceptByInput(text)
    if (conceptEntry) {
      callbacks.appendMessage({
        role: 'assistant',
        content: conceptEntry.explanation,
      })
      callbacks.setProcessing(false)
      return
    }
  }

  // ── 概念级 UB（逐子句检测）──
  // 不再因全输入中"房间号"命中配置层而跳过"当前有哪些模型"的概念检查。
  // 对每个非 MODIFY 子句独立查 bizKnowledge boundaryKeywords → ASK。
  //
  // D38-FIX-OVERAGGRESSIVE: 子句包含查询/修改模式（"有哪些""列出""改成"）时跳过概念检查，
  // 因为这些是具体配置操作而非概念性问题（如"弹幕是什么"才触发概念 ASK）。
  if (ctx.clauses && ctx.clauses.length > 0) {
    for (const clause of ctx.clauses) {
      if (/改成|设为|调整为|修改|新增|删除/.test(clause)) continue
      // 跳过显式查询/列表模式 — 这些是配置查询而非概念性疑问
      if (/有哪些|有几项|查看|列出|看下|当前.*配置|当前.*模型|当前.*设置|当前.*选项/.test(clause)) continue
      const clauseEntry = findConceptByInput(clause)
      if (clauseEntry) {
        const question = clauseEntry.clarificationQuestion.replace('{concept}', clauseEntry.displayName)
        const candidates: Array<{ label: string; configKey: string; aiHint: string }> = [
          { label: clauseEntry.displayName, configKey: clauseEntry.conceptId, aiHint: clauseEntry.explanation.substring(0, 80) + '…' },
        ]
        for (const key of clauseEntry.relatedConfigKeys) {
          const label = (globalThis as any).KEY_LABELS?.[key] || key.split('.').pop() || key
          candidates.push({ label, configKey: key, aiHint: '' })
        }

        ctx.awaitingConfirmation = true
        ctx.suspendedState = {
          question,
          candidates,
          originalSubject: clause,
          originalUserInput: text,
          fallbackMessage: `抱歉，我没有找到「${clause}」相关的信息。`,
          intent: 'QUERY',
          subject: clause,
          value: null,
          planSteps: [],
        }
        if (callbacks.onSuspend) {
          callbacks.onSuspend({ ...ctx.suspendedState, sessionState: ctx.sessionState })
        }
        callbacks.setProcessing(false)
        return
      }
    }
  }

  // 兜底：全输入概念级 UB（无子句拆分时回退到原逻辑）
  // D38-FIX-OVERAGGRESSIVE: 全输入含查询/修改模式时跳过
  if (configCandidates.length === 0 && conceptHit === null &&
      !/有哪些|有几项|查看|列出|看下|改成|设为|调整为/.test(text)) {
    const conceptEntry = findConceptByInput(text)
    if (conceptEntry) {
      const question = conceptEntry.clarificationQuestion.replace('{concept}', conceptEntry.displayName)
      const candidates: Array<{ label: string; configKey: string; aiHint: string }> = [
        { label: conceptEntry.displayName, configKey: conceptEntry.conceptId, aiHint: conceptEntry.explanation.substring(0, 80) + '…' },
      ]
      // 添加关联配置项作为候选
      for (const key of conceptEntry.relatedConfigKeys) {
        const label = (globalThis as any).KEY_LABELS?.[key] || key.split('.').pop() || key
        candidates.push({ label, configKey: key, aiHint: '' })
      }

      ctx.awaitingConfirmation = true
      ctx.suspendedState = {
        question,
        candidates,
        originalSubject: text,
        originalUserInput: text,
        fallbackMessage: `抱歉，我没有找到「${text}」相关的信息。`,
        intent: 'QUERY',
        subject: text,
        value: null,
        planSteps: [],
      }
      if (callbacks.onSuspend) {
        callbacks.onSuspend({ ...ctx.suspendedState, sessionState: ctx.sessionState })
      }
      callbacks.setProcessing(false)
      return
    }
  }

  // ── D38-10: RouteDecision 分层路由（Skill 短路分流）────────────────
  // 在进入 UA 之前，先做 RouteDecision 分流：
  //   1. 所有子句都是 high-confidence directExecution → 跳过 UA，直接执行
  //   2. 否则 → retrievalQueries 走正常 UA 路径
  // D38-10-INV-03: 概念短路/UB 是独立前置守卫，此处分流只处理 Skill 和 Config
  if (ctx.clauses && ctx.clauses.length > 0) {
    const routeDecision = buildRouteDecision(ctx.clauses)
    const allDirect = routeDecision.directExecutions.length === ctx.clauses.length

    if (allDirect && routeDecision.directExecutions.length > 0) {
      // ── 全部分子句都是高置信 skill → 跳过 UA，直接执行 ──
      console.log('[D38-10] RouteDecision all-direct skip UA:', formatRouteDecision(routeDecision))
      ctx.isUA = false
      ctx.uaSuccess = false

      // 构建 planSteps + toolCalls，直接执行
      let stepId = 0
      const planSteps: import('../types').PlanStep[] = routeDecision.directExecutions.map((de) => ({
        id: stepId++,
        text: de.clause,
        done: false,
      }))

      const toolCallsToExecute: import('../types').ToolCall[] = []
      const planStepToolRanges: number[][] = []

      for (const de of routeDecision.directExecutions) {
        const rangeStart = toolCallsToExecute.length
        for (const toolName of de.toolChain) {
          const objArgs: Record<string, unknown> = {}
          if (de.targetConfigKey) objArgs.key = de.targetConfigKey
          if (toolName === 'list_directory' && de.targetConfigKey) {
            objArgs.path = de.targetConfigKey
          }
          toolCallsToExecute.push({
            id: `tool_${toolCallsToExecute.length}`,
            type: 'function',
            function: {
              name: toolName,
              arguments: JSON.stringify(objArgs),
            },
          })
        }
        planStepToolRanges.push([rangeStart, toolCallsToExecute.length - 1])
      }

      ctx.toolCallsToExecute = toolCallsToExecute
      ctx.planStepToolRanges = planStepToolRanges
      ctx.uaUserMessage = ''

      // 执行
      const executionResult = await executeStage(
        toolCallsToExecute,
        planSteps,
        planStepToolRanges,
        false,
        '',
      )

      // 构建回复
      const replyLines: string[] = []
      const skillNames = routeDecision.directExecutions.map(de => de.skillId)
      if (skillNames.includes('skill:list_all_config')) {
        replyLines.push(`✅ 已列出所有配置项。`)
      }
      if (skillNames.includes('skill:list_model_dir')) {
        replyLines.push(`✅ 已列出模型目录下的文件。`)
      }
      if (replyLines.length === 0) {
        replyLines.push(...toolResultsText(toolCallsToExecute, executionResult.toolResults))
      }
      const reply = replyLines.join('\n').trim() || '✅ 已完成。'

      callbacks.appendMessage({ role: 'assistant', content: reply })
      callbacks.setProcessing(false)
      return
    }

    // mixed case: 部分 directExecution + 部分 retrievalQuery → 记录 decision 供后续参考
    // （retrievalQueries 部分继续走正常 UA 路径）
    if (routeDecision.directExecutions.length > 0) {
      console.log('[D38-10] RouteDecision mixed:', formatRouteDecision(routeDecision))
    }
  }

  // ── Stage 2: 意图上下文（②③④⑤⑥⑦⑩a）─────────────────────────
  await executeStageIntent(ctx)

  // ── Core: ⑩b 理解Agent + ⑩c 映射 + ⑪ 降级 ─────────────────────
  const uaResult = await executeUnderstandingAgentPath(ctx, callbacks)
  if (uaResult.outcome === 'all_chat') {
    // 展示咨询Agent 回复（all_chat 提前 return，需在此处显示）
    if (ctx.chatAnswer) {
      callbacks.appendMessage({ role: 'assistant', content: ctx.chatAnswer })
    }
    callbacks.setProcessing(false)
    return
  }
  if (uaResult.outcome === 'config_rejected') {
    // 展示咨询Agent 回复（如有：混合意图中 action 被拒但 chat 已产出）
    if (ctx.chatAnswer) {
      callbacks.appendMessage({ role: 'assistant', content: ctx.chatAnswer })
    }
    callbacks.appendMessage({ role: 'assistant', content: uaResult.reason })
    callbacks.setProcessing(false)
    return
  }
  if (uaResult.outcome === 'l1_unresolved') {
    // L1 未命中：挂起管线，等待用户选择
    if (ctx.chatAnswer) {
      callbacks.appendMessage({ role: 'assistant', content: ctx.chatAnswer })
    }
    // 设置挂起状态，等待用户回复确认
    ctx.awaitingConfirmation = true
    ctx.suspendedState = {
      question: `「${uaResult.unresolvedItems[0]}」未找到对应配置项，您是指：`,
      candidates: uaResult.candidates,
      originalSubject: uaResult.unresolvedItems[0],
      originalUserInput: ctx.userInput,
      fallbackMessage: `抱歉，当前没有「${uaResult.unresolvedItems[0]}」这个配置项。`,
      intent: 'LOOKUP',
      subject: uaResult.unresolvedItems[0],
      value: null,
      planSteps: [],
    }
    if (callbacks.onSuspend) {
      callbacks.onSuspend({ ...ctx.suspendedState, sessionState: ctx.sessionState })
    }
    callbacks.setProcessing(false)
    return
  }
  if (uaResult.outcome === 'ambiguous') {
    // 专题七：歧义主动澄清 — Top-1/Top-2 置信度过低，系统问用户
    if (ctx.chatAnswer) {
      callbacks.appendMessage({ role: 'assistant', content: ctx.chatAnswer })
    }
    ctx.awaitingConfirmation = true
    ctx.suspendedState = {
      question: uaResult.question,
      candidates: uaResult.candidates,
      originalSubject: uaResult.originalSubject,
      originalUserInput: ctx.userInput,
      fallbackMessage: '已取消查询。',
      intent: uaResult.intent,
      subject: uaResult.subject,
      value: uaResult.value,
      planSteps: [],
    }
    if (callbacks.onSuspend) {
      callbacks.onSuspend({ ...ctx.suspendedState, sessionState: ctx.sessionState })
    }
    callbacks.setProcessing(false)
    return
  }
  if (uaResult.outcome === 'lookup_unresolved') {
    // LOOKUP 三路未匹配：生成 ASK 工具调用，挂起管线
    if (ctx.chatAnswer) {
      callbacks.appendMessage({ role: 'assistant', content: ctx.chatAnswer })
    }

    // D38-4-OPT-A: 路径字段为空 → 定制 question
    const isPathEmpty = uaResult.intent === '__path_empty'
    const question = isPathEmpty
      ? `当前「${uaResult.originalSubject}」路径为空，请问是否设置路径后查看模型文件？`
      : `您的想法是？`
    const fallbackMessage = isPathEmpty
      ? '已取消查询。'
      : `抱歉，当前没有「${uaResult.originalSubject}」这个配置项。`

    // 设置挂起状态，等待用户回复确认
    ctx.awaitingConfirmation = true
    ctx.suspendedState = {
      question,
      candidates: uaResult.candidates,
      originalSubject: uaResult.originalSubject,
      originalUserInput: ctx.userInput,
      fallbackMessage,
      intent: uaResult.intent,
      subject: uaResult.subject,
      value: uaResult.value,
      planSteps: [],
    }
    // 通知 UI 存储挂起状态
    if (callbacks.onSuspend) {
      callbacks.onSuspend({ ...ctx.suspendedState, sessionState: ctx.sessionState })
    }
    callbacks.setProcessing(false)
    return
  }

  let planSteps: PlanStep[]
  let isUA: boolean

  if (uaResult.outcome === 'success') {
    planSteps = uaResult.planSteps
    isUA = true
  } else {
    // 理解Agent 失败 → 降级到旧 LLM 路径（⑪）
    planSteps = await executeFallbackPath(ctx, callbacks)
    isUA = false
  }

  // 无工具调用且是咨询类 → 已由降级路径展示回复
  if (ctx.toolCallsToExecute.length === 0) {
    callbacks.setProcessing(false)
    return
  }

  // ── Stage 3: 工具执行 + 证据链（⑫a~⑫d + ⑬ + ⑭）─────────────
  const execResult = await executeStage(
    ctx.toolCallsToExecute,
    planSteps,
    ctx.planStepToolRanges,
    isUA,
    ctx.cleanContent,
  )

  // ── 批量持久化：收集所有 write_config_value 成功结果，走 attach batch 路径 ──
  //   对齐 BlessStar App → Adapter → Kernel 三层设计：App 层收集变更，Adapter 层 gate + persist
  //   每个管线只提交一次 commitBatch，避免逐工具调用造成的低频度 WAL 写
  const writes: Array<{ key: string; value: string }> = []
  for (let i = 0; i < ctx.toolCallsToExecute.length; i++) {
    const tc = ctx.toolCallsToExecute[i]
    const tr = execResult.toolResults[i]
    if (tc.function.name === 'write_config_value' && tr?.success && tr.data) {
      const d = tr.data as { key?: string; value?: string; written?: { key: string; value: string } }
      const entry = d.written || { key: d.key || '', value: String(d.value || '') }
      if (entry.key) writes.push(entry)
    }
  }
  if (writes.length > 0) {
    const entries = JSON.stringify(writes.map(w => ({ key: w.key, value: w.value })))
    // 异步提交，不阻塞渲染；失败静默处理（运行时值已在内存中生效）
    window.blessstar.commitBatch(entries).catch(err => console.warn('[Pipeline] commitBatch 失败:', err))
    // ── 版本注册表：追加配置版本记录（第33天 · RV-01）────────────────
    import('../versionRegistry').then(({ addVersionEntries }) => {
      return addVersionEntries(writes, text)
    }).then(({ newVersionIds }) => {
      callbacks.onWriteEntries?.(writes, newVersionIds)
    }).catch(err => console.warn('[Pipeline] 版本保存失败:', err))
  }

  // ⑫c: 设置 suggestion — 仅对 action 工具（产生可编辑代码/配置）
  const ACTION_TOOLS = new Set([
    'write_config_value',
    'update_gate_rule',
    'create_schema_field',
    'generate_normalizer_template',
    'create_gate_chain',
  ])
  let suggestionSet = false
  for (let i = execResult.toolResults.length - 1; i >= 0; i--) {
    const r = execResult.toolResults[i]
    const toolName = execResult.toolCards[i]?.toolName
    if (r.success && r.data && toolName && ACTION_TOOLS.has(toolName)) {
      callbacks.setSuggestion(
        typeof r.data === 'string' ? r.data : JSON.stringify(r.data, null, 2),
      )
      suggestionSet = true
      break
    }
  }
  if (!suggestionSet) {
    callbacks.setSuggestion(null)
  }

  // 更新 lastToolDelta
  callbacks.lastToolDeltaRef.current = execResult.toolDelta

  // D38-8-方案3：工具摘要记录（保留最近 2 轮）
  if (ctx.sessionState && execResult.toolDelta) {
    const summaries = ctx.sessionState.toolSummaries
    summaries.push(`[${toolLabel(ctx.toolCallsToExecute[0]?.function.name || '')}] ${execResult.toolDelta.summary}`)
    // 仅保留最近 2 轮
    if (summaries.length > 2) {
      summaries.splice(0, summaries.length - 2)
    }
  }

  // ⑬ 证据链验证
  if (!execResult.verifyResult.allMatched) {
    const unmatched = execResult.verifyResult.unmatchedSteps.map(s => s.text).join('、')
    callbacks.appendMessage({
      role: 'assistant',
      content: `⚠️ 证据链不完整：${unmatched} 未找到 Registry 记录`,
    })
  }

  // ⑭ 编造检测
  if (execResult.fabricationWarning) {
    callbacks.appendMessage({ role: 'assistant', content: execResult.fabricationWarning })
    callbacks.setProcessing(false)
    return
  }

  // ── Stage 4: 展示 + 反馈 + 汇报（⑮⑯⑰）────────────────────────
  // ⑮ 结果展示 — 将 toolCards 挂到 assistant 消息（SandboxTodo 渲染详情）
  callbacks.updateLastAssistant((msg) => ({
    ...msg,
    toolCards: execResult.toolCards,
  }))

  // ⑯ FeedbackCollector
  recordFeedback(callbacks.feedbackRef.current, ctx.toolCallsToExecute, execResult.allToolSuccess)

  // ⑰ Wrap-up 总结 — 自然语言汇报，不重复列出详情
  if (isUA) {
    await wrapUpUA(ctx, planSteps, execResult, callbacks)
  } else {
    if (execResult.allToolSuccess && (planSteps?.length ?? 0) > 1) {
      await wrapUpFallback(text, ctx.toolCallsToExecute, execResult, callbacks)
    }
  }

  // 咨询Agent 回复延迟到沙箱后展示
  if (ctx.chatAnswer) {
    callbacks.appendMessage({ role: 'assistant', content: ctx.chatAnswer })
  }

  callbacks.setProcessing(false)
}

// ── 自然语言思考模板（PIPELINE-19：UA 路径的 thinking 从 raw 改为自然语言） ─

const OPERATION_VERB: Record<string, string> = {
  list: '列出',
  write: '修改',
  read: '读取',
  gate: '校验',
  schema: '管理',
  search: '搜索',
  lookup: '查找',
  modify: '修改',
  query_list: '列出',
  query_single: '读取',
  query_enum: '列出枚举',
  rule: '规则管理',
  validate: '校验',
  browse: '浏览',
  exec: '执行',
  generate: '生成',
  add_field: '新增',
  search_find: '搜索',
}

/** 描述一个 todo 项的自然语言动作（用于意图描述） */
function describeAction(item: { subject: string; intent: string; value: string | null; is_chat: boolean }): string {
  if (item.is_chat) return `咨询"${item.subject}"`
  const verb = OPERATION_VERB[item.intent.toLowerCase()] || item.intent
  const valuePart = item.value ? `为"${item.value}"` : ''
  return `${verb}"${item.subject}"${valuePart}`
}

/** 描述调用工具的短语（用于"我需要调用..."段落） */
function describeToolPhrase(item: { subject: string; intent: string; value: string | null; is_chat: boolean }): string {
  if (item.is_chat) return `[CHAT]工具向用户解释"${item.subject}"`
  const op = item.intent.toUpperCase()
  const verb = OPERATION_VERB[item.intent.toLowerCase()] || item.intent
  const valuePart = item.value ? `为"${item.value}"` : ''
  return `[${op}]工具${verb}"${item.subject}"${valuePart}`
}

/** 构建步骤行：{N}，[{OP}]{动词}"{主体}"{值后缀} */
function stepLine(item: { subject: string; intent: string; value: string | null; is_chat: boolean }, index: number): string {
  if (item.is_chat) return `${index}，[CHAT]向用户解释"${item.subject}"`
  const op = item.intent.toUpperCase()
  const verb = OPERATION_VERB[item.intent.toLowerCase()] || item.intent
  const valuePart = item.value ? `为"${item.value}"` : ''
  return `${index}，[${op}]${verb}"${item.subject}"${valuePart}`
}

/** 从用户输入 + 全部 todo 项构建自然语言思考模板 */
function buildThinkingTemplate(
  userInput: string,
  allItems: Array<{ subject: string; intent: string; value: string | null; is_chat: boolean }>,
): string {
  const count = allItems.length
  const lines: string[] = []

  // 第一段：用户描述 + 意图理解 + 工具调用
  const descs = allItems.map(describeAction)
  if (count === 1) {
    lines.push(`用户描述"${userInput}"，我理解为一个意图，用户需要${descs[0]}，我需要调用${describeToolPhrase(allItems[0])}。`)
  } else {
    const needsPart = descs.slice(0, -1).join('，') + '，最后' + descs[descs.length - 1]
    const toolDesc = allItems.map((item, i) => {
      const phrase = describeToolPhrase(item)
      if (i === 0) return phrase
      if (i === count - 1) return `最后调用${phrase}`
      return `然后再调用${phrase}`
    }).join('，')
    lines.push(`用户描述"${userInput}"，我理解为${count}个意图，用户需要${needsPart}，我需要调用${toolDesc}。`)
  }

  // 第二段：证据链分析
  lines.push('根据分析，证据链完整，启动沙箱，步骤为：')

  // 第三步：步骤列表
  allItems.forEach((item, i) => {
    lines.push(stepLine(item, i + 1))
  })

  return lines.join('\n')
}

// ── ⑩b⑩c 理解Agent 路径（专题七：检索增强版）────────────────────────

async function executeUnderstandingAgentPath(
  ctx: PipelineContext,
  callbacks: PipelineCallbacks,
): Promise<UAResult> {
  try {
    // ── Step 0: 按子句拆分的多路检索（D38-FIX-PER-CLAUSE）─────────
    // 对多意图输入，逐子句独立做 BM25 评分，避免混合输入全局评分失真。
    const perClauseResult = await retrievePerClause(ctx.userInput)

    // ── Step 1: 构建带候选注入的 UA Prompt ──
    const uaPrompt = buildUAPromptWithCandidates(perClauseResult.combinedInjectedContext, ctx.sessionState?.toolSummaries)
    const uaResponse = await callbacks.getBridge().complete({
      messages: [
        { role: 'system', content: uaPrompt },
        { role: 'user', content: ctx.uaUserMessage },
      ],
      temperature: ctx.thinkLevel.suggestedTemperature,
    })

    const uaRaw = uaResponse.message.content || ''
    let uaOutput = parseUnderstandingAgentOutput(uaRaw)

    // D38-8-方案4：格式错误重试 — UA JSON 解析失败时 temperature 0.3→0.5 重试 1 次
    if (!uaOutput || uaOutput.todo.length === 0) {
      const retryResponse = await callbacks.getBridge().complete({
        messages: [
          { role: 'system', content: uaPrompt },
          { role: 'user', content: ctx.uaUserMessage },
        ],
        temperature: 0.5, // 提高温度增加格式多样性
      })
      const retryRaw = retryResponse.message.content || ''
      uaOutput = parseUnderstandingAgentOutput(retryRaw)
    }

    if (!uaOutput || uaOutput.todo.length === 0) return { outcome: 'failed' }

    ctx.isUA = true
    ctx.uaSuccess = true

    // 快照 UA 原始解析输出，供测试断言验证
    const uaRawSnapshot = structuredClone(uaOutput)

    // ── 方案C：UA 产出后意图强校验修正 ──
    correctUAIntents(uaOutput.todo, ctx.userInput)

    // ── 管线硬过滤器（D38-HARD-DEFENSE）：消除空 subject+MODIFY ──
    // correctUAIntents 依赖 regex 匹配 userInput 整体 + 逐 item 条件判断，
    // 当 UA 输出格式抖动时（如 subject 非空、意图不精确）可能漏过。
    // 此过滤作为最后防线：任何 MODIFY/ACTION 项若 target_config_key 为 null
    // 且用户输入匹配列表模式，强制转为 QUERY_LIST。
    const hasListPattern = /^(当前)?有哪(些|几)|看看.*(?:配置|设置|选项|模型)|列出.*(?:配置|设置|选项|模型)|查看.*(?:配置|设置|选项|模型)|^(?:当前|所有).*(?:配置|设置|选项|模型)/.test(ctx.userInput)
    // 收集非 MODIFY 子句文本（用于回填 QUERY_LIST 的 subject）
    const nonModifyClauseTexts = perClauseResult.clauseResults
      .filter(cr => !/改成|设为|调整为/.test(cr.clause))
      .map(cr => cr.clause.trim())
    let listSubjectIdx = 0
    for (const item of uaOutput.todo) {
      // 规则 A: MODIFY + 无 configKey → 无效 MODIFY，按用户输入模式分类
      if (item.intent === 'MODIFY' && !item.target_config_key) {
        if (hasListPattern) {
          item.intent = 'QUERY_LIST'
          // 从对应子句回填 subject，避免思考模板显示 列出""
          if (!item.subject && listSubjectIdx < nonModifyClauseTexts.length) {
            item.subject = nonModifyClauseTexts[listSubjectIdx]
            listSubjectIdx++
          }
          // D38-FIX-AMBIGUOUS: QUERY_LIST + subject非空 + 无具体configKey
          // 但 perClauseResult 有与该 subject 领域相关的候选 → 触发 ASK 澄清
          if (item.subject && perClauseResult.combinedCandidates.length > 0) {
            const relatedCandidates = perClauseResult.combinedCandidates.filter(c =>
              item.subject.includes(c.label) ||
              c.label.includes(item.subject) ||
              (typeof c.aiHint === 'string' && c.aiHint.length > 0 &&
                c.aiHint.includes(item.subject))
            )
            if (relatedCandidates.length > 0) {
              item.is_ambiguous = true
            }
          }
        } else {
          item.intent = 'QUERY_SINGLE'
        }
        item.is_chat = false
        if (!item.is_ambiguous) {
          item.is_ambiguous = false
        }
      }
      // 规则 B: new_value 截断 — 去除尾部逗号/句号/换行后的污染文本
      // UA 可能将"改成10041，当前有哪些模型"整段提取为 value，截断到首个分隔符
      if (item.new_value && item.new_value.length > 0) {
        const cleanMatch = item.new_value.match(/^([^，,；;。.!！\n]+)/)
        if (cleanMatch && cleanMatch[1] !== item.new_value) {
          item.new_value = cleanMatch[1].trim()
        }
      }
    }

    // ── Per-item 分流：chat 项 → 咨询Agent，action 项 → 工具 ──
    const chatItems = uaOutput.todo.filter(t => t.is_chat)
    const rawActionItems = uaOutput.todo.filter(t => !t.is_chat)

    // 不再使用全局 BM25 置信度作为阻断条件（D38-FIX-PER-CLAUSE）。
    // UA 按子句独立判断，检索结果仅作为辅助参考。
    // 若用户输入匹配列表查询模式且 rawActionItems 为空，合成 LIST。（兜底）
    // D38-9-INV-02: 使用 QUERY_LIST 子意图
    if (rawActionItems.length === 0 && /^(当前)?有哪(些|几).*配置|看看.*配置|列出.*配置|查看.*配置|当前配置/.test(ctx.userInput)) {
      rawActionItems.push({
        subject: '列出所有配置',
        intent: 'QUERY_LIST',
        value: null,
        condition: null,
        is_chat: false,
        target_config_key: null,
        new_value: null,
        is_ambiguous: false,
        is_dangerous: false,
      })
    }

    // ── 构建 ActionItems：通过路由层（D38-7-INV-05）──
    const actionItems: Array<{
      subject: string
      operation: string
      intent: string
      value: string | null
      condition: string | null
      is_chat: boolean
      meta?: import('./stage-execute').ActionItemMeta
    }> = []

    for (const item of rawActionItems) {
      const targetKey = item.target_config_key || undefined

      // D38-FIX-AMBIGUOUS: 通用歧义检测 — 对任何 LIST/QUERY_LIST 操作，
       // 无具体 configKey 但 perClause 有领域相关候选 → ASK 澄清
       // 场景："当前有哪些模型" → UA 正确输出 QUERY_LIST 但无 configKey
       // → 不应 LIST 全部 40 项，应 ASK"你是指哪个具体配置？"
       // ✅ 硬过滤器标记 + ❌ 原生 QUERY_LIST 都需要检查
       // 注意：raw UA 输出时 item.operation 尚未赋值（由后续 routeIntentToOperations 产生），
       // 因此只检查 item.intent
       const isListNoKey = !targetKey && item.intent === 'QUERY_LIST'
      if (isListNoKey && item.subject && perClauseResult.combinedCandidates.length > 0) {
        // 按领域关键词筛选：提取子句中可能与 domain 相关的词
        const subjectDomainHints = extractDomainHints(item.subject)
        const relatedCandidates = perClauseResult.combinedCandidates.filter(c => {
          // D38-10-FIX: QUERY_LIST 不兼容非文件/目录类型
          if (!isIntentCompatibleWithValueType('QUERY_LIST', c.valueType)) return false
          const domainPrefix = c.configKey.split('.').slice(0, 2).join('.')
          const label = c.label || c.configKey
          const hint = (c.aiHint || '') + ' ' + label
          if (subjectDomainHints.some(kw => label.includes(kw))) return true
          if (item.subject.includes(label) || hint.includes(item.subject)) return true
          if (subjectDomainHints.some(kw => domainPrefix.includes(kw))) return true
          return false
        })
        if (relatedCandidates.length > 0) {
          // 对每个候选做 QUERY_LIST 确定性路由：directory/file→BROWSE_DIR, 其余→READ
          // 输出完整 [TOOL] + [configKey] 配对，让用户看到具体会执行什么操作
          const lines: string[] = []
          const enrichedCandidates: Array<{ label: string; configKey: string; aiHint: string }> = []
          for (let i = 0; i < relatedCandidates.length; i++) {
            const c = relatedCandidates[i]
            // D38-10-FIX: 用 routeQueryByValueType 确定操作（替代手动 isDirType）
            const opName = routeQueryByValueType('QUERY_LIST', c.valueType, true)
            const verb = opName === 'BROWSE_DIR' ? '列出' : '查看'
            const detail = opName === 'BROWSE_DIR' ? `${c.label}下的所有文件` : `${c.label}的值`
            lines.push(`${i + 1}，${verb}${detail}（[${opName}] + [${c.label}]）`)
            enrichedCandidates.push({
              label: `[${opName}] ${c.label}`,
              configKey: c.configKey,
              aiHint: c.aiHint,
            })
          }
          return {
            outcome: 'ambiguous',
            question: `「${item.subject}」指的是？\n${lines.join('\n')}`,
            candidates: enrichedCandidates,
            originalSubject: item.subject,
            intent: item.intent,
            subject: item.subject,
            value: null,
          }
        }
      }

      // D38-FIX-AMBIGUOUS: QUERY_LIST + subject非空 + 无具体configKey
      // 但 perClauseResult 有候选 → 触发 ASK 澄清（而非盲目 LIST 所有 40 项）
      // 仅当候选跨多个 domain（真正歧义）时才 ASK；单域内直接路由
      if (item.is_ambiguous && item.subject) {
        const relatedCandidates = perClauseResult.combinedCandidates.filter(c => {
          // D38-10-FIX: intent 与 valueType 不兼容的排除
          if (!isIntentCompatibleWithValueType(item.intent, c.valueType)) return false
          return item.subject.includes(c.label) ||
            c.label.includes(item.subject) ||
            (typeof c.aiHint === 'string' && c.aiHint.length > 0 &&
              c.aiHint.includes(item.subject))
        })
        // 统计候选跨域数：≥2 才真歧义
        const domains = new Set(relatedCandidates.map(c => c.configKey.split('.').slice(0, 2).join('.')))
        if (domains.size >= 2 && relatedCandidates.length > 1) {
          // 同格式：逐候选路由 + [TOOL] + [label] 配对
          const lines: string[] = []
          const enrichedCandidates: Array<{ label: string; configKey: string; aiHint: string }> = []
          for (let i = 0; i < relatedCandidates.length; i++) {
            const c = relatedCandidates[i]
            const opName = routeQueryByValueType(item.intent, c.valueType, true)
            const verb = opName === 'BROWSE_DIR' ? '列出' : '查看'
            const detail = opName === 'BROWSE_DIR' ? `${c.label}下的所有文件` : `${c.label}的值`
            lines.push(`${i + 1}，${verb}${detail}（[${opName}] + [${c.label}]）`)
            enrichedCandidates.push({ label: `[${opName}] ${c.label}`, configKey: c.configKey, aiHint: c.aiHint })
          }
          return {
            outcome: 'ambiguous',
            question: `「${item.subject}」指的是？\n${lines.join('\n')}`,
            candidates: enrichedCandidates,
            originalSubject: item.subject,
            intent: item.intent,
            subject: item.subject,
            value: null,
          }
        }
        // 单域内：不是真歧义，reset ambiguous，正常路由
        item.is_ambiguous = false
      }

      // D38-9: UA 新格式可能没有 subject 字段（仅有 target_config_key），
      // 自动从 LABEL_TO_KEY 补全 subject，避免 L1 匹配时空字符串导致「」未找到
      if (!item.subject && targetKey) {
        item.subject = keyToLabel(targetKey)
      }
      // 从检索候选获取 value_type（使用合并总候选）
      const valueType = targetKey ? getValueTypeFromCandidates(perClauseResult.combinedCandidates, targetKey) : undefined
      const semanticGroup = targetKey ? inferSemanticGroup(targetKey) : 'config'

      // 使用路由层决策操作列表
      const ops = routeIntentToOperations(item.intent, targetKey, valueType, ctx.userInput)

      // 检查 is_dangerous：危险操作 → ASK 确认
      if (item.is_dangerous) {
        // 危险操作先推入 ASK 等待确认（此处简化：直接输出 ask_user 工具）
        actionItems.push({
          subject: item.subject,
          operation: 'ASK',
          intent: item.intent,
          value: `即将执行危险操作「${item.subject}」，是否确认？`,
          condition: item.condition,
          is_chat: false,
        })
        continue
      }

      // 根据 targetKey 是否存在决定是否注入 key
      for (let oi = 0; oi < ops.length; oi++) {
        const op = ops[oi]
        const isLastOp = oi === ops.length - 1

        // 对 file 类型 QUERY：BROWSE_DIR 操作
        if (op === 'BROWSE_DIR' && targetKey) {
          let dirPath = item.new_value || item.value || ''
          if (!dirPath) {
            try {
              const valResult = await (window as any).blessstar?.executeTool?.('read_config_value', { key: targetKey })
              dirPath = String(valResult?.result || '')
            } catch { /* 读不到值时不阻塞 */ }
          }
          actionItems.push({
            subject: item.subject,
            operation: 'BROWSE_DIR',
            intent: item.intent,
            value: dirPath || targetKey,
            condition: isLastOp ? item.condition : null,
            is_chat: false,
            meta: { configType: 'file', domainPrefix: targetKey.split('.').slice(0, 2).join('.') },
          })
          continue
        }

        // file 类型 MODIFY：前置 BROWSE 让 Agent 先看文件列表
        if (valueType === 'file' && item.intent === 'MODIFY' && oi === 0) {
          let dirPath = ''
          try {
            const valResult = await (window as any).blessstar?.executeTool?.('read_config_value', { key: targetKey })
            dirPath = String(valResult?.result || '')
          } catch { /* 读不到值时不阻断 */ }
          if (dirPath) {
            actionItems.push({
              subject: item.subject,
              operation: 'BROWSE',
              intent: item.intent,
              value: dirPath,
              condition: null,
              is_chat: false,
              meta: { configType: 'file', domainPrefix: targetKey?.split('.').slice(0, 2).join('.') },
            })
          }
        }

        actionItems.push({
          subject: item.subject,
          operation: op,
          intent: item.intent,
          value: isLastOp ? (item.new_value || item.value || null) : null,
          condition: isLastOp ? item.condition : null,
          is_chat: false,
          meta: {
            configType: valueType,
            domainPrefix: targetKey?.split('.').slice(0, 2).join('.'),
          },
        })
      }
    }

    // 处理 chat 项：合并一起问咨询Agent，存入 ctx 延迟展示
    if (chatItems.length > 0) {
      const chatReport = await callbacks.getBridge().complete({
        messages: [
          { role: 'system', content: getConsultationPrompt(
            (BusinessAdapterRegistry.getMergedAIData() as any).consultationKnowledge || ''
          ) },
          { role: 'user', content: `用户问了以下概念性问题：\n${chatItems.map(t => `- ${t.subject}`).join('\n')}\n\n请分别解答。` },
        ],
        temperature: ctx.thinkLevel.suggestedTemperature,
      })
      ctx.chatAnswer = chatReport.message.content || null
    }

    // 全部为 chat → 无需继续
    if (actionItems.length === 0) {
      return { outcome: 'all_chat' }
    }

    // 构建 action 项的 planSteps
    const planSteps: PlanStep[] = actionItems.map((item, i) => ({
      id: i + 1,
      text: `[${item.operation}] ${item.subject}${item.value ? ` → ${item.value}` : ''}`,
      done: false,
    }))

    // 将 chat 项也加入 planSteps（标记已完成），使步数与思考模板一致
    chatItems.forEach((item) => {
      planSteps.push({
        id: planSteps.length + 1,
        text: `[CHAT] 向用户解释"${item.subject}"`,
        done: true,
      })
    })

    // L1 per-subject 精准检索：LABEL_TO_KEY 精确匹配（不含语义模糊匹配）
    const subjectToKey: Record<string, string> = {}
    const unresolved: string[] = []  // 三步兜底全失败的 subject 列表

    // D38-9-hotfix: UA 可能不输出 subject 也不输出 target_config_key，
    // 尝试从 combinedCandidates 或用户输入回填（比让 UA 自己澄清更可靠）
    for (const item of actionItems) {
      if (item.subject) continue
      // LIST/BROWSE_DIR 操作：直接从非 MODIFY 子句提取主题
      if (item.operation === 'LIST' || item.operation === 'BROWSE_DIR') {
        const listClause = perClauseResult.clauseResults.find(cr =>
          !/改成|设为|调整为/.test(cr.clause) &&
          // 匹配操作对应的意图来源（如 QUERY_LIST 对应"有哪"匹配项）
          (ctx.userInput.includes(cr.clause) || cr.clause.includes(ctx.userInput.split(/[，,；;]/)[0]))
        )
        if (listClause) {
          item.subject = listClause.clause.trim()
        }
        continue
      }
      if (SYSTEM_SCOPED_OPS.has(item.operation)) continue

      // 1. 从 combinedCandidates 的 label/aiHint 匹配用户输入
      const candidateMatch = perClauseResult.combinedCandidates.find(c =>
        ctx.userInput.includes(c.label) ||
        (typeof c.aiHint === 'string' && c.aiHint.length > 0 && ctx.userInput.includes(c.aiHint) && c.aiHint.length >= 2)
      )
      if (candidateMatch) {
        item.subject = candidateMatch.label
        continue
      }

      // 2. 兜底：从 LABEL_TO_KEY 的 label 直接匹配用户输入关键词
      const labelMatch = Object.keys(LABEL_TO_KEY).find(l =>
        l.length >= 2 && ctx.userInput.includes(l)
      )
      if (labelMatch) {
        item.subject = labelMatch
      }
    }

    for (const item of actionItems) {
      // 系统级操作（LIST/BROWSE/SEARCH 等）无需 configKey 映射，跳过 L1
      if (SYSTEM_SCOPED_OPS.has(item.operation)) continue

      let configKey: string | null = null
      // LABEL_TO_KEY 精确匹配（只接受完全一致的 label → key 映射）
      configKey = LABEL_TO_KEY[item.subject] ?? null
      // 无精确匹配 → 回问用户
      if (!configKey) {
        unresolved.push(item.subject)
        continue
      }
      subjectToKey[item.subject] = configKey

      const opError = validateOperationForConfig(configKey, item.operation)
      if (opError) {
        return { outcome: 'config_rejected', reason: opError }
      }
    }

    // L1 未命中 → 回问用户澄清（不静默用 subject 当 key）
    if (unresolved.length > 0) {
      const uniqueSubjects = [...new Set(unresolved)]
      const candidateLabels = Object.keys(LABEL_TO_KEY)
      const suggestionLines: string[] = []
      const enrichedCandidates: Array<{ label: string; configKey: string; aiHint: string }> = []
      for (const subj of uniqueSubjects) {
        const related = candidateLabels.filter(l => l.includes(subj) || subj.includes(l))
        for (const label of related) {
          const configKey = LABEL_TO_KEY[label]
          enrichedCandidates.push({ label, configKey, aiHint: AI_HINTS[configKey] || '' })
        }
        let line = `- 「${subj}」未找到对应配置项`
        if (related.length > 0) {
          const numbered = related.map((r, i) => `「${i + 1}」${r}`).join('、')
          line += `，您是指：${numbered}？`
          line += `\n  请回复数字选择对应的配置项，系统将自动把「${subj}」作为该配置项的别名`
        } else {
          line += `；当前可用配置项：${candidateLabels.slice(0, 8).map(r => `「${r}」`).join('、')}等`
        }
        suggestionLines.push(line)
      }
      const suggestions = `未找到以下配置项：\n${suggestionLines.join('\n')}\n\n如果都不符合您的预期，那么目前在当前的配置表中没有您需要的配置项。`
      return { outcome: 'l1_unresolved', unresolvedItems: uniqueSubjects, candidates: enrichedCandidates, suggestions }
    }

    // 保存原始 UA 输出（用于思考模板，不含意图展开）
    const originalTodoItems = uaOutput.todo.map(t => ({
      subject: t.subject,
      intent: t.intent,
      value: t.value,
      is_chat: !!t.is_chat,
    }))

    // ⑩c 映射层：四元组 → ToolCall（G4 修复：自动填充 value/condition + key）
    const { toolCallsToExecute, planStepToolRanges } = mapTripletsToToolCalls(actionItems, subjectToKey)
    ctx.toolCallsToExecute = toolCallsToExecute
    ctx.planStepToolRanges = planStepToolRanges

    // 自然语言思考模板（PIPELINE-19）：使用原始 UA todo 项（不展开），保持意图描述准确
    const thinkingText = buildThinkingTemplate(ctx.userInput, originalTodoItems)
    callbacks.appendMessage({
      role: 'assistant',
      content: '', // 不在气泡中显示 raw [LIST]/[WRITE]
      planSteps,
      thinking: thinkingText,
      uaRawOutput: uaRawSnapshot, // UA 原始输出快照（含 rawText），供测试验证 UA 分类准确性
    })

    ctx.cleanContent = planSteps.map(s => s.text).join('\n')
    return { outcome: 'success', planSteps }
  } catch {
    return { outcome: 'failed' }
  }
}

// ── ⑪ 降级路径：旧单 LLM 调用 ────────────────────────────────────────

async function executeFallbackPath(
  ctx: PipelineContext,
  callbacks: PipelineCallbacks,
): Promise<PlanStep[]> {
  const text = ctx.userInput

  // ── ⑧ 多子句检测（降级路径）──
  const { clauses, isMultiClause } = detectMultiClause(text)
  ctx.clauses = clauses
  ctx.isMultiClause = isMultiClause

  // ── ⑨ is_chat 检测（降级路径）──
  ctx.isChatQuery = detectChatQuery(text, ctx.skillMatch)

  // ── ②③④ 意图上下文（降级路径：三元组压缩 + 分片加载 + Token预算）──
  const { compressed, effectiveIndex } = await executeIndexShardLoad(text)
  ctx.compressed = compressed
  ctx.effectiveIndex = effectiveIndex

  // ── ⑤⑥⑦ 降级路径 L1 盲查 ──
  await executeLegacyL1ForFallback(ctx)

  let systemPrompt = SYSTEM_PROMPT

  if (ctx.compressed) {
    systemPrompt += `\n\n[意图压缩] 操作=${ctx.compressed.operation} 领域=${ctx.compressed.config.domain}${ctx.compressed.rule ? ` 规则=${ctx.compressed.rule}` : ''}${ctx.compressed.config.value ? ` 值=${ctx.compressed.config.value}` : ''}`
  }
  if (ctx.l0Hint) {
    systemPrompt += `\n[L0 Hint] 用户意图关联操作: ${ctx.l0Hint.operationHint}（${ctx.l0Hint.subjectHint}）`
  }
  if (ctx.cPathFields.length > 0) {
    systemPrompt += `\n[C路径候选] 可能相关字段: ${ctx.cPathFields.join(', ')}`
  }

  // D38-8-方案3：工具摘要记录（最近 2 轮）
  if (ctx.sessionState?.toolSummaries && ctx.sessionState.toolSummaries.length > 0) {
    const summaryLines = ctx.sessionState.toolSummaries.join('\n')
    systemPrompt += `\n\n[历史工具调用]\n${summaryLines}`
  }

  // G5 修复：降级路径 compactIndex 对齐 PIPELINE-12（仅 fieldSemantics）
  const ctxMessages = buildContext({
    userInput: text,
    systemPrompt,
    toolDefs: ctx.toolDefs,
    indexCompact: ctx.effectiveIndex ? {
      fieldSemantics: ctx.effectiveIndex.fieldSemantics,
      domainKnowledge: ctx.effectiveIndex.domainKnowledge,
      constraintKnowledge: '', // G5: 不注入 constraintKnowledge
    } : null,
    lastToolDelta: callbacks.lastToolDeltaRef.current,
    historyMessages: callbacks.getMessages().slice(-6),
  })

  const response = await callbacks.getBridge().complete({
    messages: ctxMessages,
    tools: ctx.isChatQuery ? undefined : ctx.toolDefs,
    temperature: ctx.thinkLevel.suggestedTemperature,
  })

  const rawContent = response.message.content || ''
  const planSteps = extractPlanSteps(rawContent) || []
  const thinking = extractThinking(rawContent)
  ctx.cleanContent = removePlanTags(removeThinkingLine(rawContent))

  // Collect tool calls
  ctx.toolCallsToExecute = []
  if (response.tool_calls && response.tool_calls.length > 0) {
    ctx.toolCallsToExecute.push(...response.tool_calls)
  } else if (ctx.toolMatch.tools && ctx.toolMatch.tools.length > 0 && !ctx.isChatQuery) {
    const ts = Date.now()
    ctx.toolMatch.tools.forEach((toolName, ti) => {
      ctx.toolCallsToExecute.push({
        id: `call_bpath_${ts}_${ti}`,
        type: 'function',
        function: { name: toolName, arguments: '{}' },
      })
    })
  }

  // Build planStepToolRanges for fallback (1:1 mapping)
  ctx.planStepToolRanges = ctx.toolCallsToExecute.map((_, i) => [i])

  callbacks.appendMessage({
    role: 'assistant',
    content: ctx.cleanContent,
    planSteps: planSteps.length > 0 ? planSteps : undefined,
    thinking,
  })

  return planSteps
}

// ── ⑰ Wrap-up 辅助 ──────────────────────────────────────────────────

async function wrapUpUA(
  ctx: PipelineContext,
  planSteps: PlanStep[],
  execResult: Awaited<ReturnType<typeof executeStage>>,
  callbacks: PipelineCallbacks,
): Promise<void> {
  if (planSteps.length === 0) return

  // D38-4-INV-05: 回复Agent 替代 wrapUp 硬编码
  // 收集执行结果，调用回复Agent 生成自然语言回复
  const allOk = planSteps.every((_s, ps) => {
    const indices = ctx.planStepToolRanges[ps] || []
    return indices.every(idx => execResult.toolResults[idx]?.success)
  })

  // D38-9: 从 planSteps 提取实际 operation，替代硬编码 LOOKUP
  // planSteps 格式为 "[READ] 房间号" 或 "[WRITE] 房间号 → 10041"
  const firstOpMatch = planSteps[0]?.text?.match(/^\[(\w+)\]/)
  const firstOp = firstOpMatch ? firstOpMatch[1] : ''
  // 操作→回复意图映射（与 reply.ts 一致）
  const opToReplyIntent: Record<string, string> = {
    READ: 'LOOKUP',
    WRITE: 'MODIFY',
    LIST: 'LIST',
    BROWSE_DIR: 'BROWSE',
    BROWSE: 'BROWSE',
    ADD_FIELD: 'ADD_FIELD',
    SET_RULE: 'RULE',
    SEARCH: 'SEARCH_FIND',
    FIND: 'SEARCH_FIND',
    EXEC: 'EXEC',
    GENERATE: 'GENERATE',
    VALIDATE: 'VALIDATE',
  }
  const replyIntent = opToReplyIntent[firstOp] || (ctx.uaSuccess ? 'LOOKUP' : 'SYSTEM')

  const summaryInput = JSON.stringify({
    intent: replyIntent,
    subject: planSteps[0]?.text?.replace(/^\[.*?\]\s*/, '') || '',
    value: null,
    toolResults: ctx.toolCallsToExecute.map((tc, i) => ({
      toolName: tc.function.name,
      success: execResult.toolResults[i]?.success ?? false,
      data: execResult.toolResults[i]?.data ?? null,
      error: execResult.toolResults[i]?.error ?? null,
    })),
  })

  try {
    const summaryResp = await callbacks.getBridge().complete({
      messages: [
        { role: 'system', content: REPLY_AGENT_PROMPT },
        { role: 'user', content: summaryInput },
      ],
      temperature: 0.3,
    })
    if (summaryResp.message.content) {
      callbacks.appendMessage({
        role: 'assistant',
        content: summaryResp.message.content.replace(/^['"](.*)['"]$/, '$1'),
      })
    }
  } catch {
    // wrap-up 失败不影响用户体验，使用兜底简单消息
    const fallback = allOk ? '已完成。' : '部分步骤执行失败，请重试。'
    callbacks.appendMessage({ role: 'assistant', content: fallback })
  }
}

async function wrapUpFallback(
  text: string,
  toolCallsToExecute: PipelineContext['toolCallsToExecute'],
  execResult: Awaited<ReturnType<typeof executeStage>>,
  callbacks: PipelineCallbacks,
): Promise<void> {
  const summaryMsg = [
    `原始请求：${text}`,
    `工具执行结果：${toolResultsText(toolCallsToExecute, execResult.toolResults)}`,
  ].join('\n')
  try {
    const summaryResp = await callbacks.getBridge().complete({
      messages: [
        { role: 'system', content: `你是 ${BusinessAdapterRegistry.getSystemPromptIdentity()} 汇报助手。请用一句自然语言总结工具执行结果给用户。不要调用工具，不要输出[PLAN]。不超过 2 句话。` },
        { role: 'user', content: summaryMsg },
      ],
    })
    if (summaryResp.message.content) {
      callbacks.appendMessage({
        role: 'assistant',
        content: summaryResp.message.content.replace(/^['"](.*)['"]$/, '$1'),
      })
    }
  } catch {
    // wrap-up 失败不影响用户体验
  }
}

// ── Re-export types ───────────────────────────────────────────────────

export type { PipelineContext } from './types'
export { createPipelineContext } from './types'

// ── 模块初始化：种子概念关键词（D38-8-方案6）─────────────────────

/**
 * 在模块首次加载时，将 bizKnowledge 中的冷启动概念 boundaryKeywords
 * 写入 adaptiveIndex baseline，使概念可学习。
 * 仅在 baseline 中不存在该 entry 时写入（幂等）。
 */
function initConceptSeeds(): void {
  const concepts = getAllConcepts()
  const seeds: Array<{ keyword: string; conceptId: string }> = []
  for (const c of concepts) {
    for (const kw of c.boundaryKeywords) {
      seeds.push({ keyword: kw, conceptId: c.conceptId })
    }
  }
  if (seeds.length > 0) {
    seedConceptKeywords(seeds)
  }
}

// ── 工具函数 ───────────────────────────────────────────────────────

/**
 * 从子句文本中提取领域关键词提示（用于候选筛选）。
 *
 * 将"当前有哪些模型" → ["模型"]
 *    "弹幕设置" → ["弹幕"]
 * 返回常用关键词列表。
 */
function extractDomainHints(subject: string): string[] {
  if (!subject) return []
  const hints: string[] = []
  // 已知领域关键词前缀
  const domainPrefixes = ['弹幕', '模型', 'live2d', '房间', '窗口', '布局',
    '显示', '渲染', '连接', '重连', '认证', 'cookie', '登录', '黑名单',
    '屏蔽', '动画', '位置', '置顶', '待机', '动作', '点击', '缩放',
    '内存', '测试', '新建', '新增']
  for (const prefix of domainPrefixes) {
    if (subject.includes(prefix)) {
      hints.push(prefix)
    }
  }
  // 如果没有任何领域词命中，返回空数组（后续不匹配任何候选）
  return hints
}

// 冷启动：模块加载时执行一次
initConceptSeeds()
