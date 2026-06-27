/**
 * pipeline/stage-intent — ②③④ 意图解析 + ⑩a 理解Agent消息构建（精简版）
 *
 * Stage 2：RAG 路径仅做 Think Level 选择 + L0 hint 注入。
 *
 * 专题七优化（D38-OPT-STAGE12）：② compressIntent / ③ indexShardLoad / ④ tokenBudget
 * 已从 executeStageIntent 分离为独立导出函数 executeIndexShardLoad，
 * RAG 路径不再执行（检索层替代），仅在降级路径按需调用。
 *
 * 保留函数：
 *   executeStageIntent   — Think Level + ⑩a（RAG + 降级共用）
 *   executeIndexShardLoad — ②③④ 合并导出（降级路径用）
 *   executeLegacyL1ForFallback — ⑤⑥⑦ L1 盲查（降级路径用）
 *   parseUnderstandingAgentOutput / correctUAIntents — UA 解析+修正
 */

import { selectThinkLevel } from '../context-manager/thinkLevelSelector'
import { compressIntent } from '../intent/trie_matcher'
import { selectIndexShardsSemantic, extractDomainKeyPrefixes, shardResultToCompactIndex, getAllDomains } from '../context-manager/indexShardLoader'
import { estimateContextTokens } from '../context-manager/tokenBudget'
import { loadInvertedIndex, scoredRetrieve } from '../context-manager/fieldRetriever'
import { matchTools } from '../tools/toolMatcher'
import { getToolDefinitions } from '../tools'
import { mockFieldSelection } from '../context-manager/llmFieldSelector'
import { SYSTEM_PROMPT } from '../prompts/system'
import type { PipelineContext, CollectedHints } from './types'

// ═══════════════════════════════════════════════════════════════════════════
// executeStageIntent（精简版：仅 Think Level + ⑩a）
// ═══════════════════════════════════════════════════════════════════════════

/**
 * 执行 Stage Intent（精简版）：Think Level 选择 + ⑩a L0 hint 注入。
 *
 * ② compressIntent / ③ indexShardLoad / ④ tokenBudget 已移至 executeIndexShardLoad，
 * 仅在降级路径按需调用。
 */
export async function executeStageIntent(ctx: PipelineContext): Promise<void> {
  const text = ctx.userInput

  // ── Think Level 选择（RAG + 降级共用）──
  const routingCtx = { userInput: text, skillRouterEnabled: true, metaModeEnabled: false }
  ctx.routingCtx = routingCtx
  ctx.thinkLevel = selectThinkLevel(text, routingCtx)

  // ── ⑩a L0 采集 + 构建理解Agent消息 ──
  ctx.hints = collectHints(ctx.l0Hint)
  ctx.uaUserMessage = buildUAUserMessage(text, ctx.hints)

  // ② compressIntent / ③ indexShardLoad / ④ tokenBudget → 见 executeIndexShardLoad
}

// ═══════════════════════════════════════════════════════════════════════════
// ②③④ 合并导出（降级路径用）
// ═══════════════════════════════════════════════════════════════════════════

/**
 * 降级路径：执行 ② compressIntent + ③ indexShardLoad + ④ tokenBudget。
 *
 * RAG 路径跳过此步骤（由检索层替代），降级路径在 fallback 中调用。
 */
export async function executeIndexShardLoad(
  text: string,
): Promise<{
  compressed: ReturnType<typeof compressIntent>
  effectiveIndex: PipelineContext['effectiveIndex']
}> {
  // ── ② 三元组压缩 ──
  const compressed = compressIntent(text)

  // ── ③ Compact Index 分片加载（P2+EMB: 语义匹配优先，回退到关键字）──
  const shards = await selectIndexShardsSemantic(text, getAllDomains())
  void extractDomainKeyPrefixes(shards)
  const compactIndex = shardResultToCompactIndex(shards)

  // ── ④ Token 预算预检 + 超限降级 ──
  let effectiveIndex: PipelineContext['effectiveIndex'] = compactIndex
  if (compactIndex) {
    const tokenEstimate = estimateContextTokens(SYSTEM_PROMPT, text, compactIndex, null)
    if (tokenEstimate.overBudget) {
      effectiveIndex = {
        fieldSemantics: compactIndex.fieldSemantics,
        domainKnowledge: compactIndex.domainKnowledge,
        constraintKnowledge: '', // 超限时裁掉 constraint（对齐 PIPELINE-12 零知识原则）
      }
    }
  }

  return { compressed, effectiveIndex }
}

// ═══════════════════════════════════════════════════════════════════════════
// ⑤⑥⑦ 降级路径 L1（已切换至检索层 BM25）
// ═══════════════════════════════════════════════════════════════════════════

import { retrieveTopKCandidates } from './retriever'
import { findConceptByInput } from './bizKnowledge'

/**
 * 执行降级路径检索 + Tool Matcher + C 路径降级。
 *
 * 仅在降级路径（UA 失败/无 LLM）中调用。
 * 使用检索层（retriever.ts）的 BM25 多路召回替代旧 AdaptiveIndex.query()。
 *
 * D38-FIX-ADAPTIVE-RETIRE: 降级路径切到 retrieveTopKCandidates，
 * 废弃 adaptiveIndex.query() 的直接调用（仍保留其数据存储层供 confirmMatch 用）。
 */
export async function executeLegacyL1ForFallback(ctx: PipelineContext): Promise<void> {
  const text = ctx.userInput

  // ── ⑤ L1 检索层 BM25 查询 ──
  const result = await retrieveTopKCandidates(text, 5)
  ctx.adaptiveResults = result.candidates.map((c) => ({
    configKey: c.configKey,
    label: c.label,
    aiHint: c.aiHint,
    score: c.score,
    matchedBy: '',
    source: 'baseline' as const,
    tools: [] as string[],
    semanticType: 'config_value' as const,
  }))
  ctx.fieldScores = scoredRetrieve(loadInvertedIndex(), text, 10)

  // ── ⑥ B 路径：Tool Matcher ──
  ctx.toolMatch = matchTools(text)
  ctx.toolDefs = getToolDefinitions()

  // ── D38-8-方案5：概念锚点链 ──
  // 当 BM25 检索无命中 + Tool Matcher 无结果时，
  // 查 bizKnowledge 相关概念，将其 relatedConfigKeys 作为候选注入。
  let anchorInjected = false
  if (ctx.toolMatch.tools.length === 0 && ctx.adaptiveResults.length === 0) {
    const conceptEntry = findConceptByInput(text)
    if (conceptEntry && conceptEntry.relatedConfigKeys.length > 0) {
      for (const configKey of conceptEntry.relatedConfigKeys) {
        ctx.adaptiveResults.push({
          configKey,
          label: configKey.split('.').pop() || configKey,
          aiHint: '',
          score: 0.5,
          matchedBy: 'concept_anchor',
          source: 'baseline',
          tools: [],
          semanticType: 'config_value',
        })
      }
      anchorInjected = true
    }
  }

  // ── ⑦ C 路径 LLM 字段选择器（B 路径无结果且检索也无时降级，且锚点链未注入）─
  if (ctx.toolMatch.tools.length === 0 && ctx.adaptiveResults.length === 0 && !anchorInjected) {
    ctx.cPathFields = mockFieldSelection({
      userInput: text,
      candidateFields: ctx.fieldScores.map(f => f.field),
      topK: 5,
    }).selectedFields
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// 辅助函数
// ═══════════════════════════════════════════════════════════════════════════

/** 仅 L0 采集（L1 后置到检索层，不再注入理解Agent） */
export function collectHints(
  l0Hint: PipelineContext['l0Hint'],
): CollectedHints {
  return { l0: l0Hint, l1: null }
}

/** 构建理解Agent的 user message（用户原文 + 仅 L0 已知信息） */
export function buildUAUserMessage(text: string, hints: CollectedHints): string {
  const parts: string[] = [`## 用户描述\n${text}`]

  const infoParts: string[] = []
  if (hints.l0) {
    infoParts.push(`- 用户意图的操作类型是 ${hints.l0.operationHint}`)
    infoParts.push(`- 操作目标域是 ${hints.l0.subjectHint}`)
  }
  if (infoParts.length > 0) {
    parts.push(`## 已知信息\n${infoParts.join('\n')}`)
  }

  return parts.join('\n\n')
}

/** 解析理解Agent的 JSON 输出（兼容新旧格式），容错处理 */
export function parseUnderstandingAgentOutput(raw: string): import('./types').UnderstandingAgentOutput | null {
  try {
    const jsonMatch = raw.match(/\{[\s\S]*"todo"[\s\S]*\}/)
    if (!jsonMatch) return null
    const parsed = JSON.parse(jsonMatch[0])
    if (!parsed.todo || !Array.isArray(parsed.todo)) return null
    return {
      todo: parsed.todo.map((item: Record<string, unknown>) => ({
        subject: item.subject || '',
        // 兼容新旧字段：优先取 intent，回退到 operation
        intent: String(item.intent || item.operation || 'QUERY'),
        value: (item.value as string) ?? null,
        condition: (item.condition as string) ?? null,
        is_chat: !!item.is_chat,
        // 新字段（专题七）
        target_config_key: (item.target_config_key as string) ?? null,
        new_value: (item.new_value as string) ?? null,
        is_ambiguous: !!item.is_ambiguous,
        is_dangerous: !!item.is_dangerous,
      })),
    }
  } catch {
    return null
  }
}

/**
 * UA 产出后意图强校验修正（方案C — 专题七精简版：4 条规则）。
 *
 * D38-7-INV-06: 方案C 兜底规则从 7 条缩减到 3 条，本次新增规则4（列表查询兜底）。
 * 规则4 覆盖检索层无候选时 LLM 将"当前有哪些配置"等列表查询误判为 chat 的场景。
 */
export function correctUAIntents(
  todo: Array<{
    subject: string
    intent: string
    value: string | null
    condition: string | null
    is_chat: boolean
    target_config_key?: string | null
    new_value?: string | null
    is_ambiguous?: boolean
    is_dangerous?: boolean
  }>,
  userInput: string,
): void {
  for (const item of todo) {
    // 规则1: 输入含"新增/创建/加一个字段" → 强制 MODIFY（ADD_FIELD 作为路由层子操作）
    // 注意：排除"如何/怎么/怎样"等疑问前缀，避免将咨询类意图误转为 MODIFY
    if (/新增|(?<!如何|怎么|怎样|为何|爲何)创建|加(一个)?.*字段|加配置项/.test(userInput)) {
      if (item.intent !== 'MODIFY') {
        item.intent = 'MODIFY'
        item.is_chat = false
        continue
      }
    }

    // 规则2: 输入含"改成/设为/调整为"+ 非空目标 → 强制 MODIFY（仅非 MODIFY 意图时修正）
    if (item.intent !== 'MODIFY') {
      const modifyMatch = userInput.match(/(改成|设为|调整为)\s*(\S+)/)
      if (modifyMatch && modifyMatch[2]) {
        item.intent = 'MODIFY'
        item.new_value = item.new_value || item.value || modifyMatch[2]
        item.is_chat = false
        continue
      }
    }

    // 规则3: 输入含"列出目录/浏览文件" → 强制 QUERY_LIST（路由层自动选 BROWSE_DIR）
    if (item.intent === 'ACTION' && /列出.*目录|浏览.*文件|有什么文件/.test(userInput)) {
      item.intent = 'QUERY_LIST'
      item.is_chat = false
      continue
    }

    // 规则4: 输入匹配列表查询模式 → 强制 QUERY_LIST
    // 覆盖场景：
    //   - LLM 误判为 ACTION/chat（原逻辑）
    //   - LLM 误判为 MODIFY 且 subject/configKey 为空（如 UA 全量输出 MODIFY）
    if (/^(当前)?有哪(些|几)|看看.*(?:配置|设置|选项)|列出.*(?:配置|设置|选项)|查看.*(?:配置|设置|选项)|^(?:当前|所有).*(?:配置|设置|选项)/.test(userInput)) {
      if (item.is_chat || item.intent === 'ACTION' || (item.intent === 'MODIFY' && !item.subject && !item.target_config_key)) {
        item.intent = 'QUERY_LIST'
        item.is_chat = false
        item.target_config_key = null  // 列表查询不需要特定 configKey
        item.is_ambiguous = false      // 列表查询意图清晰，强制无歧义（D38-FIX-LOOP-2）
        continue
      }
    }

    // 规则5（D38-9-INV-02）: 输入含"哪几档/可选范围/可选值"等枚举查询模式
    // → 强制 QUERY_ENUM。即使 LLM 误判为 QUERY/chat 也修正。
    if (/哪几档|可选范围|可选值|支持什么.*(格式|分辨率|尺寸)/.test(userInput)) {
      if (item.intent !== 'QUERY_ENUM') {
        item.intent = 'QUERY_ENUM'
        item.is_chat = false
        continue
      }
    }
  }
}
