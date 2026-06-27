/**
 * skillRouter — Skill Router（L0 层）
 *
 * 专题六：/command 前缀确定性路由 + L0 hint 采集。
 * 专题十（D38-10）：自然语言 Skill 注册表 + RouteDecision 分层路由。
 * 专题十一（D38-11）：合并 /command + NL 注册表 → UnifiedSkillDefinition；
 *   /command → 意图（而非 tool），走命令门控 + 确定性意图路由。
 */

import { TOOL_TO_OPERATION, routeQueryByValueType, isIntentCompatibleWithValueType } from '../operationMapper'
import { BusinessAdapterRegistry } from '../business-adapter/registry'
import { LABEL_TO_KEY } from '../tools/configLabels'
import { retrievePerClause } from '../pipeline/retriever'

// ═══════════════════════════════════════════════════════════════════════
// 专题十一：统一 Skill 注册表（Single Source of Truth）
// ═══════════════════════════════════════════════════════════════════════

export type MatchStrategy = 'exact' | 'includes'

/**
 * 单一真相源 — 同时覆盖 /command 和自然语言路由。
 * exactCommands: 精确前缀匹配（/list /read /write）
 * nlKeywords: 自然语言触发词（列出所有配置/有哪些配置）
 * intent: /command 映射为确定意图（QUERY_LIST / QUERY_SINGLE / MODIFY）
 * executor: toolChain，保留向后兼容
 */
export interface UnifiedSkillDefinition {
  id: string
  executor: string[]
  triggers: {
    exactCommands: string[]
    nlKeywords: Array<{ text: string; matchStrategy: MatchStrategy }>
  }
  intent: string
  description: string
  source: 'generic' | 'business'
  approvalRequired?: boolean
}

// ── 统一注册表（所有技能的定义）───────────────────────────────
export const UNIFIED_SKILLS: UnifiedSkillDefinition[] = [
  {
    id: 'skill:list_all_config',
    executor: ['list_configs'],
    triggers: {
      exactCommands: ['/list', '/ls'],
      nlKeywords: [
        { text: '列出所有配置', matchStrategy: 'exact' },
        { text: '列出所有字段', matchStrategy: 'exact' },
        { text: '列出所有参数', matchStrategy: 'exact' },
        { text: '列出所有项', matchStrategy: 'exact' },
        { text: '有哪些配置', matchStrategy: 'includes' },
        { text: '有哪些字段', matchStrategy: 'includes' },
        { text: '有哪些参数', matchStrategy: 'includes' },
        { text: '有哪些项', matchStrategy: 'includes' },
      ],
    },
    intent: 'QUERY_LIST',
    description: '列出所有配置项',
    source: 'generic',
  },
  {
    id: 'skill:read_config',
    executor: ['read_config_value'],
    triggers: {
      exactCommands: ['/read'],
      nlKeywords: [],
    },
    intent: 'QUERY_SINGLE',
    description: '查看配置值',
    source: 'generic',
  },
  {
    id: 'skill:write_config',
    executor: ['read_config_value', 'write_config_value'],
    triggers: {
      exactCommands: ['/write', '/set'],
      nlKeywords: [],
    },
    intent: 'MODIFY',
    description: '修改配置值',
    source: 'generic',
    approvalRequired: true,
  },
  {
    id: 'skill:search_config',
    executor: ['search_content'],
    triggers: {
      exactCommands: ['/search'],
      nlKeywords: [],
    },
    intent: 'ACTION',
    description: '搜索配置',
    source: 'generic',
  },
  {
    id: 'skill:create_config',
    executor: ['create_schema_field', 'validate_config'],
    triggers: {
      exactCommands: ['/createconfig'],
      nlKeywords: [],
    },
    intent: 'ACTION',
    description: '创建配置字段',
    source: 'generic',
  },
  {
    id: 'skill:create_rule',
    executor: ['update_gate_rule'],
    triggers: {
      exactCommands: ['/createrule'],
      nlKeywords: [],
    },
    intent: 'ACTION',
    description: '创建 Gate 规则',
    source: 'generic',
  },
]

// ── 旧版 /command 路由表（占位保留，仅用于向后兼容 L0 hint 采集）─────
// 配置管理类命令已迁移至 UNIFIED_SKILLS 走新命令门控。
// 其他命令（findfile/terminal等）后续按需迁移。

export interface SkillRoute {
  prefix: string
  description: string
  toolChain: string[]
  priority: number
  approvalRequired?: boolean
}

/** @deprecated 新命令走 UNIFIED_SKILLS，此表仅保留供旧代码引用不报错 */
let SKILL_ROUTES: SkillRoute[] = []

/** 从 BusinessAdapterRegistry 同步业务 Skill 路由（占位保留） */
export function syncSkillsFromRegistry(): void {
  // Business Skill 路由未来按需要注入 UNIFIED_SKILLS
}

if (BusinessAdapterRegistry.initialized) {
  syncSkillsFromRegistry()
}

// ═══════════════════════════════════════════════════════════════════════
// 专题十一：命令门控 — parseCommand
// ═══════════════════════════════════════════════════════════════════════

export interface ParsedCommand {
  matched: boolean
  command: string
  intent: string
  rest: string
  value?: string
  description: string
}

/**
 * 解析 /command 输入，返回确定意图 + 参数。
 *
 * - `/list` → { intent: "QUERY_LIST", rest: "" }
 * - `/list 房间号` → { intent: "QUERY_LIST", rest: "房间号" }
 * - `/write 房间号 10041` → { intent: "MODIFY", rest: "房间号", value: "10041" }
 * - `/listxxx` → { matched: false }（精确匹配，非前缀匹配）
 */
export function parseCommand(input: string): ParsedCommand {
  if (!input || !input.startsWith('/')) {
    return { matched: false, command: '', intent: '', rest: '', description: '' }
  }

  const trimmed = input.trim()
  const firstSpace = trimmed.indexOf(' ')
  const cmdStr = firstSpace < 0 ? trimmed.slice(1) : trimmed.slice(1, firstSpace)
  const restPart = firstSpace < 0 ? '' : trimmed.slice(firstSpace + 1).trim()

  // 精确匹配 exactCommands（非前缀匹配）
  for (const skill of UNIFIED_SKILLS) {
    if (skill.triggers.exactCommands.includes('/' + cmdStr)) {
      let value: string | undefined
      let finalRest = restPart

      // MODIFY: 最后一个空格后的部分是 value
      if (skill.intent === 'MODIFY' && restPart) {
        const lastSpace = restPart.lastIndexOf(' ')
        if (lastSpace > 0) {
          value = restPart.slice(lastSpace + 1).trim()
          finalRest = restPart.slice(0, lastSpace).trim()
        } else if (lastSpace < 0 && restPart) {
          // 只有一个参数：是配置名，没有值
          finalRest = restPart
        }
      }

      return {
        matched: true,
        command: cmdStr,
        intent: skill.intent,
        rest: finalRest,
        value,
        description: skill.description,
      }
    }
  }

  return { matched: false, command: '', intent: '', rest: '', description: '' }
}

// ═══════════════════════════════════════════════════════════════════════
// 专题十一：两步参数检索
// ═══════════════════════════════════════════════════════════════════════

export interface ResolvedParam {
  found: boolean
  candidates: Array<{ configKey: string; label: string; valueType?: string }>
  source: 'exact' | 'bm25'
}

/**
 * 两步检索参数：
 * 1. 先在 configLabels（LABEL_TO_KEY）中做精确匹配
 * 2. 精确匹配不成功，用 BM25 检索全部配置项
 */
export async function resolveParam(rest: string): Promise<ResolvedParam> {
  if (!rest) return { found: false, candidates: [], source: 'exact' }

  // Step 1: 精确匹配 configLabels
  const exactKey = LABEL_TO_KEY[rest]

  // Step 2: BM25 检索（同时获取 valueType）
  try {
    const result = await retrievePerClause(rest)
    if (result.combinedCandidates && result.combinedCandidates.length > 0) {
      // BM25 有结果 → 优先用（含 valueType 信息）
      if (exactKey) {
        const exactMatch = (result.combinedCandidates as Array<{ configKey: string; label?: string; valueType?: string }>).find(c => c.configKey === exactKey)
        if (exactMatch) {
          return {
            found: true,
            candidates: [{
              configKey: exactKey,
              label: rest,
              valueType: exactMatch.valueType,
            }],
            source: 'exact',
          }
        }
      }
      return {
        found: true,
        candidates: (result.combinedCandidates as Array<{ configKey: string; label?: string; valueType?: string }>)
          .map(c => ({
            configKey: c.configKey,
            label: c.label || c.configKey,
            valueType: c.valueType,
          })),
        source: 'bm25',
      }
    }
  } catch {
    // BM25 不可用（测试环境），走精确匹配 fallback
  }

  // Fallback: 只用精确匹配（无 valueType）
  if (exactKey) {
    return {
      found: true,
      candidates: [{ configKey: exactKey, label: rest, valueType: undefined }],
      source: 'exact',
    }
  }

  return { found: false, candidates: [], source: 'bm25' }
}

// ═══════════════════════════════════════════════════════════════════════
// 专题十：自然语言 Skill 注册表（向后兼容，数据源映射到 UNIFIED_SKILLS）
// ═══════════════════════════════════════════════════════════════════════

export interface SkillRegistration {
  skillId: string
  keywords: Array<{ text: string; matchStrategy: MatchStrategy }>
  toolChain: string[]
  targetConfigKey?: string
  description: string
  source: 'generic' | 'business'
}

function getAllNLRegistrations(): SkillRegistration[] {
  return UNIFIED_SKILLS
    .filter(s => s.triggers.nlKeywords.length > 0)
    .map(s => ({
      skillId: s.id,
      keywords: s.triggers.nlKeywords,
      toolChain: s.executor,
      description: s.description,
      source: s.source,
    }))
}

/** 按 skillId 查找注册条目 */
export function getSkillRegistration(skillId: string): SkillRegistration | undefined {
  const unified = UNIFIED_SKILLS.find(s => s.id === skillId)
  if (!unified || unified.triggers.nlKeywords.length === 0) return undefined
  return {
    skillId: unified.id,
    keywords: unified.triggers.nlKeywords,
    toolChain: unified.executor,
    description: unified.description,
    source: unified.source,
  }
}

export interface NLSkillMatch {
  matched: boolean
  registration?: SkillRegistration
  matchedKeyword?: string
  confidence: number
}

export function matchNaturalLanguage(input: string): NLSkillMatch {
  if (!input) return { matched: false, confidence: 0 }
  const lower = input.trim().toLowerCase()

  for (const reg of getAllNLRegistrations()) {
    for (const kw of reg.keywords) {
      if (kw.matchStrategy === 'exact') {
        if (lower === kw.text.toLowerCase()) {
          return { matched: true, registration: reg, matchedKeyword: kw.text, confidence: 0.9 }
        }
      } else if (kw.matchStrategy === 'includes') {
        if (lower.includes(kw.text.toLowerCase())) {
          return { matched: true, registration: reg, matchedKeyword: kw.text, confidence: 0.6 }
        }
      }
    }
  }

  return { matched: false, confidence: 0 }
}

/** 获取所有 NL 注册条目（供基线注入使用） */
export function getAllNaturalLangSkills(): SkillRegistration[] {
  return getAllNLRegistrations()
}

// ═══════════════════════════════════════════════════════════════════════
// 专题十：RouteDecision 类型 + buildRouteDecision（不变）
// ═══════════════════════════════════════════════════════════════════════

export interface DirectExecution {
  clause: string
  skillId: string
  matchedKeyword: string
  confidence: number
  toolChain: string[]
  targetConfigKey?: string
}

export interface RetrievalQuery {
  clause: string
  reason: 'no_skill' | 'skill_low_confidence'
}

export interface RouteDecision {
  directExecutions: DirectExecution[]
  retrievalQueries: RetrievalQuery[]
}

export let skillConfidenceThreshold = 0.7

export function setSkillConfidenceThreshold(v: number): void {
  skillConfidenceThreshold = v
}

export function buildRouteDecision(
  clauses: string[],
  perClauseCandidates?: Map<string, Array<{ configKey: string; label?: string; aiHint?: string; valueType?: string }>>,
): RouteDecision {
  const decision: RouteDecision = { directExecutions: [], retrievalQueries: [] }

  for (const clause of clauses) {
    const nlMatch = matchNaturalLanguage(clause)

    if (!nlMatch.matched) {
      decision.retrievalQueries.push({ clause, reason: 'no_skill' })
      continue
    }

    const reg = nlMatch.registration!
    if (nlMatch.confidence < skillConfidenceThreshold) {
      let coveredByConfig = false
      if (perClauseCandidates?.has(clause)) {
        const candidates = perClauseCandidates.get(clause)!
        if (candidates.length > 0) {
          const domains = new Set(candidates.map(c => c.configKey.split('.').slice(0, 2).join('.')))
          if (domains.size <= 2) {
            coveredByConfig = true
          }
        }
      }
      if (!coveredByConfig) {
        decision.retrievalQueries.push({ clause, reason: 'skill_low_confidence' })
        continue
      }
    }

    decision.directExecutions.push({
      clause,
      skillId: reg.skillId,
      matchedKeyword: nlMatch.matchedKeyword!,
      confidence: nlMatch.confidence,
      toolChain: reg.toolChain,
      targetConfigKey: reg.targetConfigKey,
    })
  }

  return decision
}

export function formatRouteDecision(d: RouteDecision): string {
  const lines: string[] = ['RouteDecision:']
  if (d.directExecutions.length > 0) {
    lines.push('  ⚡ directExecutions:')
    for (const de of d.directExecutions) {
      lines.push(`    [${de.skillId}] "${de.clause}" → ${de.toolChain.join(' → ')} (conf=${de.confidence})`)
    }
  }
  if (d.retrievalQueries.length > 0) {
    lines.push('  🔍 retrievalQueries:')
    for (const rq of d.retrievalQueries) {
      lines.push(`    "${rq.clause}" reason=${rq.reason}`)
    }
  }
  return lines.join('\n')
}

// ═══════════════════════════════════════════════════════════════════════
// 旧版 /command 匹配（向后兼容，用于 L0 hint 采集）
// ═══════════════════════════════════════════════════════════════════════

export interface SkillMatch {
  matched: boolean
  route?: SkillRoute
  params?: string
}

export function matchSkill(input: string): SkillMatch {
  if (!input || !input.startsWith('/')) {
    return { matched: false }
  }

  const trimmed = input.trim()
  let bestMatch: SkillMatch = { matched: false }

  for (const route of SKILL_ROUTES) {
    if (trimmed.startsWith(route.prefix)) {
      const params = trimmed.slice(route.prefix.length).trim()
      if (!bestMatch.matched || route.priority > (bestMatch.route?.priority ?? 0)) {
        bestMatch = {
          matched: true,
          route,
          params: params || undefined,
        }
      }
    }
  }

  return bestMatch
}

export function getAllSkills(): SkillRoute[] {
  return [...SKILL_ROUTES]
}

export function derivePrimaryOperation(toolChain: string[]): string {
  const primary = toolChain.find(t =>
    t !== 'read_config_value' && t !== 'validate_config'
  )
  if (primary && TOOL_TO_OPERATION[primary]) return TOOL_TO_OPERATION[primary]
  return 'READ'
}

export function collectL0Hint(input: string): { operationHint: string; subjectHint: string } | null {
  const skill = matchSkill(input)
  if (!skill.matched || !skill.route) return null
  return {
    operationHint: derivePrimaryOperation(skill.route.toolChain),
    subjectHint: skill.route.description,
  }
}

export function getSkillHelpText(): string {
  return SKILL_ROUTES
    .sort((a, b) => b.priority - a.priority)
    .map((s) => `  ${s.prefix} — ${s.description}${s.approvalRequired ? ' ⚠️ 需确认' : ''}`)
    .join('\n')
}

export function registerSkill(route: SkillRoute): void {
  const existing = SKILL_ROUTES.findIndex((r) => r.prefix === route.prefix)
  if (existing >= 0) {
    SKILL_ROUTES[existing] = route
  } else {
    SKILL_ROUTES.push(route)
  }
}
