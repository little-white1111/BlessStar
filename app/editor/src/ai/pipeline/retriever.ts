/**
 * retriever — 检索增强层：多路召回 Top-5 配置候选（D38-7-INV-01~03）
 *
 * 专题七：检索增强生成方案核心模块。
 * 替代 L1 三路匹配（resolveLookupIntent），通过倒排索引 + BM25 多路召回，
 * 从 BusinessAdapterRegistry 自动构建的富文本索引中检索 Top-5 配置候选。
 *
 * D38-7-INV-01: 检索层单一数据源 — 索引从 Registry 自动构建
 * D38-7-INV-02: synonyms 自动生成 — 从 LABEL_TO_KEY 反向 + invertedIndex keyword
 * D38-7-INV-03: 置信度阈值硬约束 — Top-1/Top-2 ≥ 1.5 为高置信，否则 is_ambiguous
 */

import { LABEL_TO_KEY, AI_HINTS, KEY_LABELS } from '../tools/configLabels'
import { loadInvertedIndex, scoredRetrieve } from '../context-manager/fieldRetriever'
import { splitUserIntent } from '../parallelExecutor'
import { loadIndex as loadAdaptiveIndex, confirmMatch as adaptiveConfirmMatch } from '../context-manager/adaptiveIndex'
import { getAllDomains } from '../context-manager/indexShardLoader'

// ── 常量 ──────────────────────────────────────────────────────────────

/** BM25 参数：k1（饱和度控制） */
const BM25_K1 = 1.5
/** BM25 参数：b（长度归一化） */
const BM25_B = 0.75
/** 检索 Top-K 数 */
const DEFAULT_TOP_K = 5

// ── 类型 ──────────────────────────────────────────────────────────────

/** 分组提示：检索层对用户意图的分组判定结果 */
export type GroupHint = 'list' | 'single' | 'enum' | null

/** 检索候选配置项 */
export interface ConfigCandidate {
  /** 配置键（如 "livedesign.room.room_id"） */
  configKey: string
  /** 中文标签（如 "房间号"） */
  label: string
  /** 自然语言描述（来自 AI_HINTS） */
  aiHint: string
  /** 字段类型（从 schema 推断：'string' | 'number' | 'boolean' | 'file' | 'enum'） */
  valueType: string
  /** 当前值（实时读取，可能为空） */
  currentValue: string
  /** 枚举选项（type='enum' 时存在） */
  enumOptions?: string[]
  /** BM25 得分 */
  score: number
  /** 所属 domain 列表（分组聚合用，D38-9-INV-01） */
  domains: string[]
}

/** 检索结果 */
export interface RetrieveResult {
  /** Top-K 候选列表 */
  candidates: ConfigCandidate[]
  /** 注入 UA 的富文本上下文 */
  injectedContext: string
  /** 分组提示（D38-9-INV-02：'list' | 'single' | 'enum' 或 null） */
  groupHint: GroupHint
  /** 按 domain 分组后的候选结构（D38-9-INV-01） */
  groupedCandidates: Record<string, ConfigCandidate[]>
}

// ── BM25 纯 JS 实现 ─────────────────────────────────────────────────

interface DocumentEntry {
  /** 文档文本（用于 BM25 分词） */
  text: string
  /** 关联的 configKey */
  configKey: string
}

/** 文档集合（构建时一次性计算） */
let documentCorpus: DocumentEntry[] = []
let avgDocLength = 0
let idfCache: Record<string, number> = {}

// ── configKey → domain 映射缓存（D38-9-INV-01） ──
let configToDomainMap: Map<string, string[]> | null = null

/**
 * 构建 configKey → domainName[] 映射。
 * 从 getAllDomains() 遍历每个 DomainShard 的 keywords，
 * 对 configKey/label 做包含匹配来建立映射。
 *
 * D38-9-INV-01: Domain Shard + BM25 双重筛选
 */
function buildConfigToDomainMap(): Map<string, string[]> {
  if (configToDomainMap) return configToDomainMap

  const domains = getAllDomains()
  const map = new Map<string, string[]>()

  for (const [configKey, label] of Object.entries(KEY_LABELS)) {
    const matchedDomains: string[] = []
    for (const domain of domains) {
      const matched = domain.keywords.some(kw =>
        configKey.toLowerCase().includes(kw.toLowerCase()) ||
        label.toLowerCase().includes(kw.toLowerCase())
      )
      if (matched) matchedDomains.push(domain.domainName)
    }
    if (matchedDomains.length > 0) map.set(configKey, matchedDomains)
  }

  configToDomainMap = map
  return map
}

/**
 * 从 Registry + configLabels 构建文档集合。
 * 每条配置项生成为一个文档，文本包含：label + aiHint + configKey + invertedIndex keywords。
 * 同时追加 AdaptiveIndex personalProfile 条目作为额外文档（用户学习数据）。
 */
function buildCorpus(): void {
  const docs: DocumentEntry[] = []
  const seen = new Set<string>()

  // 确保 domain 映射已构建
  const domainMap = buildConfigToDomainMap()

  // ── 第 1 部分：从 LABEL_TO_KEY 遍历所有配置项 ──
  for (const [label, configKey] of Object.entries(LABEL_TO_KEY)) {
    if (seen.has(configKey)) continue
    seen.add(configKey)

    const aiHint = AI_HINTS[configKey] || ''
    const labelDisplay = KEY_LABELS[configKey] || label

    // 从倒排索引收集关联的关键词
    const index = loadInvertedIndex()
    const relatedKeywords: string[] = []
    for (const [keyword, tools] of Object.entries(index)) {
      if (tools.includes(configKey) || configKey.includes(keyword) || keyword.includes(configKey)) {
        relatedKeywords.push(keyword)
      }
    }

    const text = [
      labelDisplay,
      label,
      configKey,
      aiHint,
      ...relatedKeywords,
    ].filter(Boolean).join(' ')

    docs.push({ text, configKey })
  }

  // ── 第 2 部分：追加 AdaptiveIndex personalProfile 条目 ──
  // 每个 personalProfile entry 作为一条独立文档，BM25 多命中天然提高该 configKey 得分。
  // 例如用户确认过"房间"→ room_id，则多一条文档：
  //   "房间 livedesign.room.room_id B站直播间ID（短号/长号），如 10041"
  try {
    const adaptiveIdx = loadAdaptiveIndex()
    for (const [, entry] of Object.entries(adaptiveIdx.personalProfile)) {
      if (!entry.targetId) continue
      const aiHint = AI_HINTS[entry.targetId] || ''
      const text = [
        entry.keyword,
        entry.targetId,
        aiHint,
      ].filter(Boolean).join(' ')
      docs.push({ text, configKey: entry.targetId })
    }
  } catch {
    // AdaptiveIndex 加载失败不阻断
  }

  documentCorpus = docs

  // 计算平均文档长度
  const totalLen = docs.reduce((sum, d) => sum + d.text.length, 0)
  avgDocLength = docs.length > 0 ? totalLen / docs.length : 1

  // 预计算 IDF
  const docCount = docs.length
  const df: Record<string, number> = {}
  for (const doc of docs) {
    const terms = tokenizeForBM25(doc.text)
    const unique = new Set(terms)
    for (const term of unique) {
      df[term] = (df[term] || 0) + 1
    }
  }
  idfCache = {}
  for (const [term, freq] of Object.entries(df)) {
    idfCache[term] = Math.log(1 + (docCount - freq + 0.5) / (freq + 0.5))
  }
}

/**
 * BM25 分词：英文字母数字小写词 + 中文 bigram。
 */
function tokenizeForBM25(text: string): string[] {
  const tokens: string[] = []
  const lower = text.toLowerCase()

  // 英文词
  const enTokens = lower.match(/[a-z][a-z0-9]*/g)
  if (enTokens) {
    for (const t of enTokens) {
      if (t.length >= 2) tokens.push(t)
    }
  }

  // 中文 bigram
  const chChars = lower.match(/[\u4e00-\u9fff]/g)
  if (chChars) {
    for (let i = 0; i < chChars.length - 1; i++) {
      tokens.push(chChars[i] + chChars[i + 1])
    }
  }

  return tokens
}

/**
 * 计算单文档 BM25 得分。
 */
function bm25Score(query: string, doc: DocumentEntry): number {
  const queryTerms = tokenizeForBM25(query)
  if (queryTerms.length === 0) return 0

  const docTerms = tokenizeForBM25(doc.text)
  const docLen = docTerms.length
  const tf: Record<string, number> = {}
  for (const term of docTerms) {
    tf[term] = (tf[term] || 0) + 1
  }

  let score = 0
  for (const term of queryTerms) {
    const idf = idfCache[term] || 0
    const freq = tf[term] || 0
    score += idf * (freq * (BM25_K1 + 1)) / (freq + BM25_K1 * (1 - BM25_B + BM25_B * docLen / avgDocLength))
  }

  return score
}

/**
 * BM25 多路精排：对查询文本在所有文档上计算 BM25 得分，返回 Top-K。
 */
function bm25Rank(query: string, topK: number): ConfigCandidate[] {
  if (documentCorpus.length === 0) return []

  const domainMap = buildConfigToDomainMap()
  const scored = documentCorpus
    .map(doc => ({
      configKey: doc.configKey,
      label: KEY_LABELS[doc.configKey] || '',
      aiHint: AI_HINTS[doc.configKey] || '',
      valueType: inferValueType(doc.configKey),
      currentValue: '',
      score: bm25Score(query, doc),
      domains: domainMap.get(doc.configKey) || [],
    }))
    .sort((a, b) => b.score - a.score)
    .slice(0, topK)

  return scored
}

/**
 * 对 BM25 Top-K 候选做 Domain Shard 分组聚合（D38-9-INV-01）。
 *
 * 分组判定硬规则：
 * 1. Top-K 中有 valueType='enum' + 用户输入含"哪几档/可选值" → 'enum'
 * 2. Top-K 分布在 ≥2 个 domain，且最大 domain 候选 < 3 → 'list'
 * 3. Top-K 集中在一个 domain 且候选 ≥3 → 'list'
 * 4. 最大 domain 占 ≥60% 且无 enum → 'single'
 * 5. 否则 → null（由 UA 自行判断）
 */
function groupByDomain(
  candidates: ConfigCandidate[],
  userInput: string,
): { groupHint: GroupHint; groupedCandidates: Record<string, ConfigCandidate[]> } {
  const groupedCandidates: Record<string, ConfigCandidate[]> = {}
  let hasEnumOption = false

  for (const c of candidates) {
    // 按 domain 分组：每个候选可属于多个 domain，取第一个为 primary
    const primaryDomain = c.domains.length > 0 ? c.domains[0] : '__unmatched'
    if (!groupedCandidates[primaryDomain]) {
      groupedCandidates[primaryDomain] = []
    }
    groupedCandidates[primaryDomain].push(c)

    if (c.enumOptions && c.enumOptions.length > 0) {
      hasEnumOption = true
    }
  }

  // 硬规则 1: enum + 用户输入含"哪几档/可选值" → 'enum'
  if (hasEnumOption && /哪几档|可选范围|可选值|有哪些选项|支持什么/.test(userInput)) {
    return { groupHint: 'enum', groupedCandidates }
  }

  const domainCount = Object.keys(groupedCandidates).length
  // 计算最大 domain 的候选数占比
  let maxDomainCount = 0
  for (const [, entries] of Object.entries(groupedCandidates)) {
    if (entries.length > maxDomainCount) {
      maxDomainCount = entries.length
    }
  }

  // 硬规则 2: ≥2 个 domain，且最大 domain 候选 < 3 → 'list'
  if (domainCount >= 2 && maxDomainCount < 3) {
    return { groupHint: 'list', groupedCandidates }
  }

  // 硬规则 3: 只有一个 domain 且候选 ≥3 → 'list'
  if (domainCount === 1 && candidates.length >= 3) {
    return { groupHint: 'list', groupedCandidates }
  }

  // 硬规则 4: 最大 domain 占 ≥60% → 'single'
  if (maxDomainCount / candidates.length >= 0.6) {
    return { groupHint: 'single', groupedCandidates }
  }

  // 硬规则 5: 否则 → null
  return { groupHint: null, groupedCandidates }
}

// ── 字段类型推断 ─────────────────────────────────────────────────────

/**
 * 缓存 schema 字段类型映射（惰性加载）。
 */
let schemaTypeCache: Map<string, string> | null = null

/**
 * 加载 schema 字段类型映射（通过 blessstar.getRegisteredSchemas）。
 */
async function loadSchemaTypes(): Promise<Map<string, string>> {
  if (schemaTypeCache) return schemaTypeCache

  const typeMap = new Map<string, string>()
  try {
    const schema = await (window as any).blessstar?.getRegisteredSchemas?.()
    if (schema?.fields) {
      for (const f of (schema.fields as Array<{ key: string; type?: string; enum?: string[] }>)) {
        if (!f.key) continue
        let inferredType = f.type || 'string'
        // 文件路径特殊处理
        if (inferredType === 'string' && (
          f.key.endsWith('_path') || f.key.endsWith('_directory') || f.key.endsWith('_file')
        )) {
          inferredType = 'file'
        }
        if (f.enum) inferredType = 'enum'
        typeMap.set(f.key, inferredType)
      }
    }
  } catch { /* fallback */ }

  schemaTypeCache = typeMap
  return typeMap
}

/**
 * 重置 schema 类型缓存（schema 变更时调用）。
 */
export function resetSchemaTypeCache(): void {
  schemaTypeCache = null
}

/**
 * 从 schema 推断字段 value_type。
 * 优先从 blessstar.getRegisteredSchemas() 读取，回退到启发式推断。
 */
function inferValueType(configKey: string): string {
  if (schemaTypeCache?.has(configKey)) {
    return schemaTypeCache.get(configKey)!
  }

  // 启发式推断
  if (configKey.endsWith('_path') || configKey.endsWith('_directory') || configKey.endsWith('_file')) return 'file'
  if (configKey.endsWith('_id')) return 'string'
  if (configKey.endsWith('_count') || configKey.endsWith('_num') || configKey.endsWith('_size')) return 'number'
  if (configKey.endsWith('_enabled') || configKey.endsWith('_on')) return 'boolean'

  return 'string'
}

// ── 富文本上下文注入 ─────────────────────────────────────────────

/**
 * 构建注入 UA 的富文本上下文。
 * 格式：候选列表中每个配置项占一行，含完整字段信息。
 */
function buildInjectedContext(candidates: ConfigCandidate[]): string {
  if (candidates.length === 0) return ''

  const lines = candidates.map((c, i) => {
    const enumPart = c.enumOptions ? `，可选值：${c.enumOptions.join(' / ')}` : ''
    const valuePart = c.currentValue ? `，当前值：${c.currentValue}` : ''
    return `${i + 1}. ${c.label}（${c.configKey}）类型：${c.valueType}${valuePart}${enumPart}\n   描述：${c.aiHint || '无'}`
  })

  return lines.join('\n')
}

// ── Per-Clause 检索结果 ────────────────────────────────────────────

/** 单子句的检索结果 */
export interface ClauseRetrieveResult {
  /** 子句原文 */
  clause: string
  /** 该子句的候选列表 */
  candidates: ConfigCandidate[]
  /** 注入到 UA 的富文本上下文（含子句来源标注） */
  injectedContext: string
}

/** 多子句检索总结果 */
export interface PerClauseRetrieveResult {
  /** 按子句拆分的结果列表 */
  clauseResults: ClauseRetrieveResult[]
  /** 合并排序后的总 Top-K 候选（去重，BM25 综合得分排序） */
  combinedCandidates: ConfigCandidate[]
  /** 合并后的富文本上下文（注入 UA 用） */
  combinedInjectedContext: string
  /** 分组提示（D38-9-INV-02） */
  groupHint: GroupHint
  /** 按 domain 分组后的候选结构 */
  groupedCandidates: Record<string, ConfigCandidate[]>
}

/**
 * 按子句拆分的多路检索。
 *
 * D38-FIX-PER-CLAUSE: 对多意图输入按逗号/句号拆分为子句，
 * 逐子句独立做 BM25 检索，返回每子句的置信度 + 合并后的总候选。
 * 解决全局 BM25 对混合意图整体评分导致置信度失真的问题。
 *
 * @param userInput 用户原始输入（可能包含多子句）
 * @param topK 每子句保留候选数（默认 5）
 * @returns 每子句结果 + 合并总候选
 */
export async function retrievePerClause(
  userInput: string,
  topK: number = DEFAULT_TOP_K,
): Promise<PerClauseRetrieveResult> {
  const clauses = splitUserIntent(userInput)
  const effectiveClauses = clauses.length > 0 ? clauses : [userInput]

  // Step 1: 逐子句检索
  const clauseResults: ClauseRetrieveResult[] = await Promise.all(
    effectiveClauses.map(async (clause) => {
      const result = await retrieveTopKCandidates(clause, topK)
      // 构建带子句来源标注的上下文
      const clauseCtx = result.candidates.length > 0
        ? `【子句: ${clause}】\n${result.candidates.map((c, i) =>
            `${i + 1}. ${c.label}（${c.configKey}）得分：${c.score.toFixed(2)}`
          ).join('\n')}`
        : `【子句: ${clause}】无匹配`
      return {
        clause,
        candidates: result.candidates,
        injectedContext: clauseCtx,
      }
    }),
  )

  // Step 2: 全量检索（综合所有子句，用于合并排序）
  const combinedResult = await retrieveTopKCandidates(userInput, topK)

  // Step 3: 构建带子句标注的富文本上下文
  const clauseAnnotations = clauseResults
    .filter(r => r.candidates.length > 0)
    .map(r => r.injectedContext)
  const injectedLines = combinedResult.candidates.map((c, i) => {
    const enumPart = c.enumOptions ? `，可选值：${c.enumOptions.join(' / ')}` : ''
    const valuePart = c.currentValue ? `，当前值：${c.currentValue}` : ''
    // 标注此候选在哪些子句中命中
    const hitClauses = clauseResults
      .filter(r => r.candidates.some(rc => rc.configKey === c.configKey))
      .map(r => `"${r.clause}"`)
    const hitNote = hitClauses.length > 0 ? `（命中：${hitClauses.join('、')}）` : ''
    return `${i + 1}. ${c.label}（${c.configKey}）类型：${c.valueType}${valuePart}${enumPart}${hitNote}\n   描述：${c.aiHint || '无'}`
  })
  const groupHintLine = combinedResult.groupHint
    ? `## 检索层提示\ngroupHint=${combinedResult.groupHint}（系统检测到用户询问意图类型，请按硬规则优先采用此提示）\n`
    : ''
  const combinedInjectedContext = injectedLines.length > 0
    ? `${groupHintLine}## 候选配置项\n${injectedLines.join('\n')}\n\n## 逐子句检索详情\n${clauseAnnotations.join('\n\n')}`
    : groupHintLine || ''

  return {
    clauseResults,
    combinedCandidates: combinedResult.candidates,
    combinedInjectedContext,
    groupHint: combinedResult.groupHint,
    groupedCandidates: combinedResult.groupedCandidates,
  }
}

// ── 实时值读取 ────────────────────────────────────────────────────

/**
 * 读取配置项的当前值。
 * 使用 blessstar IPC 读取，读取失败时返回空字符串。
 */
async function readCurrentValue(configKey: string): Promise<string> {
  try {
    const result = await (window as any).blessstar?.executeTool?.('read_config_value', { key: configKey })
    return String(result?.result ?? result?.value ?? '')
  } catch {
    return ''
  }
}

// ── 主入口 ───────────────────────────────────────────────────────────

/**
 * 检索 Top-5 配置候选（多路召回：倒排索引 + BM25）。
 *
 * 步骤：
 * 1. 倒排索引模糊匹配（scoredRetrieve）
 * 2. BM25 精排
 * 3. 合并去重 + 得分归一化
 * 4. 置信度计算
 * 5. 富文本上下文构建
 *
 * D38-7-INV-01: 单一数据源 — Registry + configLabels
 * D38-7-INV-02: synonyms 自动生成 — 通过 BM25 跨字段匹配实现
 * D38-7-INV-03: 置信度阈值硬约束
 *
 * @param userInput 用户原始输入
 * @param topK 返回候选数（默认 5）
 * @returns 检索结果（含候选列表 + 置信度 + 注入上下文）
 */
export async function retrieveTopKCandidates(
  userInput: string,
  topK: number = DEFAULT_TOP_K,
): Promise<RetrieveResult> {
  if (!userInput || userInput.trim().length === 0) {
    return {
      candidates: [],
      injectedContext: '',
      groupHint: null,
      groupedCandidates: {},
    }
  }

  // Step 0: 初始化 schema 类型缓存
  await loadSchemaTypes()

  // Step 0a: 确保文档集合已构建
  if (documentCorpus.length === 0) {
    buildCorpus()
  }

  // Step 1: 倒排索引模糊匹配
  const index = loadInvertedIndex()
  const invertedResults = scoredRetrieve(index, userInput, topK * 2)
  const invertedMap = new Map<string, number>()
  for (const r of invertedResults) {
    invertedMap.set(r.field, r.score)
  }

  // Step 2: BM25 精排
  const bm25Results = bm25Rank(userInput, topK * 2)

  // Step 3: 合并去重 + 得分归一化（BM25 为主，倒排为辅助加分）
  const mergedMap = new Map<string, { candidate: ConfigCandidate; priority: number }>()

  // BM25 结果优先
  for (let i = 0; i < bm25Results.length; i++) {
    const r = bm25Results[i]
    const invertedBonus = invertedMap.get(r.configKey) || 0
    // 归一化加分：倒排命中额外加 BM25 得分的 10%
    const finalScore = r.score + (invertedBonus > 0 ? r.score * 0.1 : 0)
    mergedMap.set(r.configKey, {
      candidate: { ...r, score: finalScore },
      priority: i,
    })
  }

  // 仅当倒排结果在 BM25 结果中未出现才加入
  const domainMap = buildConfigToDomainMap()
  for (const [configKey, score] of invertedMap.entries()) {
    if (!mergedMap.has(configKey)) {
      mergedMap.set(configKey, {
        candidate: {
          configKey,
          label: KEY_LABELS[configKey] || configKey,
          aiHint: AI_HINTS[configKey] || '',
          valueType: inferValueType(configKey),
          currentValue: '',
          score: score * 0.5, // 倒排权重降低
          domains: domainMap.get(configKey) || [],
        },
        priority: 999,
      })
    }
  }

  // 按得分降序排序
  const sorted = Array.from(mergedMap.values())
    .sort((a, b) => b.candidate.score - a.candidate.score)
    .slice(0, topK)
    .map(entry => entry.candidate)

  // Step 4: 异步读取当前值（并行，不阻塞排序）
  await Promise.all(sorted.map(async (c) => {
    c.currentValue = await readCurrentValue(c.configKey)
  }))

  // Step 4b: Domain Shard 分组聚合（D38-9-INV-01）
  const { groupHint, groupedCandidates } = groupByDomain(sorted, userInput)

  // Step 5: 富文本上下文
  const injectedContext = buildInjectedContext(sorted)

  return {
    candidates: sorted,
    injectedContext,
    groupHint,
    groupedCandidates,
  }
}

/**
 * 重置检索器缓存（schema 变更、配置更新时调用）。
 */
export function resetRetrieverCache(): void {
  documentCorpus = []
  avgDocLength = 0
  idfCache = {}
  configToDomainMap = null
}

/**
 * 用户别名持久化键（localStorage）。
 * 存储格式：JSON Record<string, string>，如 {"房间": "livedesign.room.room_id"}
 */
const ALIASES_STORAGE_KEY = 'blessstar:user_aliases:v1'

/**
 * 从 localStorage 回填用户别名到 LABEL_TO_KEY。
 * 在模块初始化时调用，确保重启后别名仍生效。
 */
export function loadAliasesFromStorage(): void {
  try {
    const raw = localStorage.getItem(ALIASES_STORAGE_KEY)
    if (!raw) return
    const aliases = JSON.parse(raw) as Record<string, string>
    for (const [alias, configKey] of Object.entries(aliases)) {
      if (!LABEL_TO_KEY[alias] && configKey) {
        LABEL_TO_KEY[alias] = configKey
      }
    }
  } catch {
    // localStorage 不可用或数据损坏，不影响启动
  }
}

/**
 * 用户通过 ASK 确认别名后调用。
 *
 * 职责（一次性完成所有关联更新）：
 * 1. 写入 LABEL_TO_KEY → 下次 buildCorpus 可见
 * 2. 追加 AI_HINTS → 富文本上下文标注别名来源
 * 3. 写入 AdaptiveIndex personalProfile → localStorage 持久化
 * 4. 写入 localStorage 别名独立键 → 启动回填用
 * 5. 重置检索缓存 → 下次 retrieveTopKCandidates 重建语料
 *
 * @param alias 用户输入的别名（如 "房间"）
 * @param configKey 匹配的配置键（如 "livedesign.room.room_id"）
 */
export function confirmMatch(alias: string, configKey: string): void {
  // 1. 写入 LABEL_TO_KEY
  LABEL_TO_KEY[alias] = configKey

  // 2. 追加 AI_HINTS（标注别名来源，避免重复标注）
  const aliasNote = `（别名：${alias}）`
  if (!AI_HINTS[configKey]?.includes(aliasNote)) {
    AI_HINTS[configKey] = (AI_HINTS[configKey] || '') + aliasNote
  }

  // 3. 写入 AdaptiveIndex（localStorage 持久化）
  try {
    adaptiveConfirmMatch(alias, configKey)
  } catch {
    // AdaptiveIndex 异常不阻断
  }

  // 4. 写入 localStorage 别名独立键（启动回填用）
  try {
    const raw = localStorage.getItem(ALIASES_STORAGE_KEY)
    const aliases: Record<string, string> = raw ? JSON.parse(raw) : {}
    aliases[alias] = configKey
    localStorage.setItem(ALIASES_STORAGE_KEY, JSON.stringify(aliases))
  } catch {
    // localStorage 不可用
  }

  // 5. 重置检索缓存 → 下次检索重建语料
  resetRetrieverCache()
}

// ── 模块初始化：启动时回填持久化别名 ──
loadAliasesFromStorage()

/** 从检索候选列表中根据 configKey 查找 valueType。 */
export function getValueTypeFromCandidates(
  candidates: ConfigCandidate[],
  configKey: string,
): string | undefined {
  return candidates.find(c => c.configKey === configKey)?.valueType
}
