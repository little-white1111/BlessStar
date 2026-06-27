/**
 * adaptiveIndex.ts — 多层通用学习框架（D38-8-INV-01 RouteEntry 泛化）
 *
 * 专题八升级：从配置专用三层索引 → 多层通用学习框架。
 *
 * 核心变化：
 *   1. IndexEntry → RouteEntry：configKey 泛化为 targetType + targetId
 *   2. AdaptiveIndexData version 1→2，loadIndex() 自动迁移
 *   3. queryAllLayers() + resolveRoute()：统一查询入口
 *   4. confirmRoute()：统一学习入口，支持 config / concept / skill 三种目标类型
 *
 * 权重公式：
 *   score = freq × recency_boost × source_weight
 *   recency_boost = 1 + 1 / log₂(1 + hours_since_last_hit)
 *   source_weight = 1.5 (personalProfile) | 1.2 (baseline) | 1.0 (group) | 0.7 (ai_suggested)
 *
 * 匹配优先级：personalProfile > groupProfile > baseline
 *
 * D38-8-INV-01: RouteEntry 泛化
 * D38-8-INV-02: 配置层优先
 * D38-8-INV-03: 概念短路 freq≥1
 * D38-8-INV-05: 概念初始 ≤3 个，后续由 confirmRoute 学习
 */

import { KEY_LABELS, AI_HINTS, refreshFromRegistry } from '../tools/configLabels'
import { BusinessAdapterRegistry } from '../business-adapter/registry'
import { getAllNaturalLangSkills } from './skillRouter'

// ── 配置语义类型映射 ──────────────────────────────────────────────────

/**
 * 每个 configKey 的语义类型，帮助理解Agent 区分"配置值" vs "文件/目录"。
 * 启动时由 BusinessAdapterRegistry 注入。
 */
export const CONFIG_SEMANTIC_TYPES: Record<string, 'config_value' | 'directory' | 'file_path' | 'url'> = {}

export function getSemanticType(configKey: string): 'config_value' | 'directory' | 'file_path' | 'url' {
  return CONFIG_SEMANTIC_TYPES[configKey] || 'config_value'
}

// ── Types ─────────────────────────────────────────────────────────────

export type IndexSource = 'baseline' | 'personalProfile' | 'groupProfile' | 'ai_suggested'

/** 目标类型（可扩展） */
export type TargetType = 'config' | 'concept' | 'skill'

/**
 * RouteEntry — 通用路由条目（D38-8-INV-01）
 *
 * 替代旧的 IndexEntry。targetType 区分路由去向：
 *   - 'config'  → 配置检索路径（原 configKey 语义）
 *   - 'concept' → 概念短路路径（bizKnowledge）
 *   - 'skill'   → 技能路由路径（预留扩展）
 */
export interface RouteEntry {
  /** 用户说的关键词/短语 */
  keyword: string
  /** 目标类型：config / concept / skill */
  targetType: TargetType
  /** 目标 ID：configKey / conceptId / skillId */
  targetId: string
  /** 来源 */
  source: IndexSource
  /** 命中次数 */
  freq: number
  /** 最近一次命中时间戳 (ms) */
  lastHit: number
  /** 来源权重 */
  sourceWeight: number
}

export interface AdaptiveIndexData {
  /** 数据版本：2（v1→v2 迁移后） */
  version: 2
  /** 个人配置（可混合存放 config / concept / skill 三种 route） */
  personalProfile: Record<string, RouteEntry>
  /** 群体学习（占位，二期云端） */
  groupProfile: Record<string, RouteEntry>
  /** 出厂基线（不可删除，仅降权） */
  baseline: Record<string, RouteEntry>
}

export interface QueryResult {
  configKey: string
  label: string
  aiHint: string
  score: number
  matchedBy: string
  source: IndexSource
  /** 匹配的 tool 名称列表 */
  tools: string[]
  /** 配置值的语义类型：帮助 LLM 区分"配置值" vs "文件/目录" */
  semanticType: 'config_value' | 'directory' | 'file_path' | 'url'
}

/**
 * RouteQueryResult — queryAllLayers 的返回类型
 * 包含 targetType，供 resolveRoute 分类消费
 */
export interface RouteQueryResult {
  targetType: TargetType
  targetId: string
  keyword: string
  score: number
  freq: number
  source: IndexSource
}

// ── 出厂基线关键词表 ─────────────────────────────────────────────────

/** 出厂基线关键词表：启动时由 BusinessAdapterRegistry 注入 */
const BASELINE_KW: Record<string, string[]> = {}

// ── Source weight mapping ─────────────────────────────────────────────

const SOURCE_WEIGHT: Record<IndexSource, number> = {
  personalProfile: 1.5,
  baseline: 1.2,
  groupProfile: 1.0,
  ai_suggested: 0.7,
}

// ── localStorage key ──────────────────────────────────────────────────

const STORAGE_KEY = 'blessstar:adaptive_index:v1'

// ── State ─────────────────────────────────────────────────────────────

let _index: AdaptiveIndexData | null = null

// ── Helpers ───────────────────────────────────────────────────────────

function now(): number {
  return Date.now()
}

function hoursSince(ts: number): number {
  return Math.max(0, (now() - ts) / (1000 * 60 * 60))
}

function recencyBoost(lastHit: number): number {
  const h = hoursSince(lastHit)
  if (h < 1) return 2.0 // 1 小时内翻倍
  return 1 + 1 / Math.log2(1 + h)
}

function entryScore(e: RouteEntry): number {
  const boost = recencyBoost(e.lastHit)
  return e.freq * boost * e.sourceWeight
}

function makeEntry(
  keyword: string,
  targetId: string,
  targetType: TargetType,
  source: IndexSource,
): RouteEntry {
  return {
    keyword,
    targetType,
    targetId,
    source,
    freq: 1,
    lastHit: now(),
    sourceWeight: SOURCE_WEIGHT[source],
  }
}

// ── Init ──────────────────────────────────────────────────────────────

/** 从 BusinessAdapterRegistry 同步基线数据（测试/运行时均可调用） */
export function syncBaselineFromRegistry(): void {
  const data = BusinessAdapterRegistry.getMergedAIData()
  if (data.configSemanticTypes) {
    Object.assign(CONFIG_SEMANTIC_TYPES, data.configSemanticTypes)
  }
  if (data.baselineKW) {
    for (const [keyword, keys] of Object.entries(data.baselineKW)) {
      if (!BASELINE_KW[keyword]) {
        BASELINE_KW[keyword] = keys
      }
    }
  }
}

// 如果已有注册数据，立即同步
if (BusinessAdapterRegistry.initialized) {
  syncBaselineFromRegistry()
}

function initBaseline(): Record<string, RouteEntry> {
  const entries: Record<string, RouteEntry> = {}
  // 配置层 RouteEntry（原 baselineKW）
  for (const [keyword, keys] of Object.entries(BASELINE_KW)) {
    for (const key of keys) {
      const entryKey = `${keyword}::config::${key}`
      entries[entryKey] = {
        keyword,
        targetType: 'config',
        targetId: key,
        source: 'baseline',
        freq: 0,
        lastHit: 0,
        sourceWeight: SOURCE_WEIGHT.baseline,
      }
    }
  }
  // D38-10: Skill 层 RouteEntry（注入自然语言 Skill 的 keyword → skillId）
  for (const skill of getAllNaturalLangSkills()) {
    for (const kw of skill.keywords) {
      const entryKey = `${kw.text}::skill::${skill.skillId}`
      entries[entryKey] = {
        keyword: kw.text,
        targetType: 'skill',
        targetId: skill.skillId,
        source: 'baseline',
        freq: 0,
        lastHit: 0,
        sourceWeight: SOURCE_WEIGHT.baseline,
      }
    }
  }
  return entries
}

/**
 * 迁移 v1 数据到 v2（D38-8-INV-01）
 *
 * v1 IndexEntry: { keyword, configKey, source, freq, lastHit, sourceWeight }
 * v2 RouteEntry: { keyword, targetType, targetId, source, freq, lastHit, sourceWeight }
 *
 * 迁移方式：configKey → targetId，targetType = 'config'，删除 configKey
 */
function migrateV1ToV2(parsed: Record<string, unknown>): AdaptiveIndexData {
  const oldData = parsed as {
    version?: number
    personalProfile?: Record<string, Record<string, unknown>>
    groupProfile?: Record<string, Record<string, unknown>>
    baseline?: Record<string, Record<string, unknown>>
  }

  const newData: AdaptiveIndexData = {
    version: 2,
    personalProfile: {},
    groupProfile: {},
    baseline: {},
  }

  for (const layerName of ['personalProfile', 'groupProfile', 'baseline'] as const) {
    const oldLayer = oldData[layerName]
    if (!oldLayer) continue
    for (const [entryKey, oldEntry] of Object.entries(oldLayer)) {
      const { configKey: oldConfigKey, ...rest } = oldEntry as Record<string, unknown>
      const newEntry: RouteEntry = {
        keyword: String(rest.keyword ?? ''),
        targetType: 'config',
        targetId: String(oldConfigKey ?? ''),
        source: (rest.source as IndexSource) ?? 'baseline',
        freq: Number(rest.freq ?? 0),
        lastHit: Number(rest.lastHit ?? 0),
        sourceWeight: Number(rest.sourceWeight ?? SOURCE_WEIGHT.baseline),
      }
      // 重建 entryKey 以匹配 v2 格式（keyword::targetType::targetId）
      const newKey = `${newEntry.keyword}::${newEntry.targetType}::${newEntry.targetId}`
      newData[layerName][newKey] = newEntry
    }
  }

  return newData
}

export function loadIndex(): AdaptiveIndexData {
  if (_index) return _index

  try {
    const raw = localStorage.getItem(STORAGE_KEY)
    if (raw) {
      const parsed = JSON.parse(raw)
      // 检测版本：无 version / version 1 → 迁移到 v2
      if (!parsed.version || parsed.version === 1) {
        _index = migrateV1ToV2(parsed)
        // 合并出厂基线（可能新增了关键词）
        const baseline = initBaseline()
        for (const [entryKey, entry] of Object.entries(_index.baseline)) {
          if (baseline[entryKey]) {
            baseline[entryKey] = { ...baseline[entryKey], freq: entry.freq, lastHit: entry.lastHit }
          }
        }
        _index.baseline = baseline
        saveIndex() // 写出迁移后的数据
        return _index
      }

      // v2 数据：正常加载，合并基线
      const parsedV2 = parsed as AdaptiveIndexData
      const baseline = initBaseline()
      for (const [entryKey, entry] of Object.entries(parsedV2.baseline)) {
        if (baseline[entryKey]) {
          baseline[entryKey] = { ...baseline[entryKey], freq: entry.freq, lastHit: entry.lastHit }
        }
      }
      _index = {
        version: 2,
        baseline,
        personalProfile: parsedV2.personalProfile || {},
        groupProfile: parsedV2.groupProfile || {},
      }
      return _index
    }
  } catch {
    // localStorage 不可用或数据损坏
  }

  _index = {
    version: 2,
    baseline: initBaseline(),
    personalProfile: {},
    groupProfile: {},
  }
  return _index
}

function saveIndex(): void {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(_index))
  } catch {
    // localStorage 满或不可用
  }
}

// ── 通用查询（D38-8-INV-01） ────────────────────────────────────────

/**
 * 统一查询所有三层，返回所有命中（按 score 降序）。
 *
 * @param userInput 用户原始输入
 * @returns 按 score 降序排列的所有命中
 */
export function queryAllLayers(userInput: string): RouteQueryResult[] {
  const idx = loadIndex()
  const lowerInput = userInput.toLowerCase().trim()
  if (!lowerInput) return []

  const hits: Array<{ entry: RouteEntry }> = []

  const collectFrom = (layer: Record<string, RouteEntry>) => {
    for (const [, entry] of Object.entries(layer)) {
      const lowerKw = entry.keyword.toLowerCase()
      if (lowerKw === lowerInput || lowerKw.includes(lowerInput) || lowerInput.includes(lowerKw)) {
        hits.push({ entry })
      }
    }
  }

  // 按优先级收集：personalProfile > groupProfile > baseline
  collectFrom(idx.personalProfile)
  collectFrom(idx.groupProfile)
  collectFrom(idx.baseline)

  return hits
    .map(({ entry }) => ({
      targetType: entry.targetType,
      targetId: entry.targetId,
      keyword: entry.keyword,
      score: entryScore(entry),
      freq: entry.freq,
      source: entry.source,
    }))
    .sort((a, b) => b.score - a.score)
}

/**
 * 分类解析查询结果（D38-8-INV-02：配置层优先）
 *
 * 按 targetType 将 queryAllLayers 的命中分类为 configCandidates / conceptHit / skillHit。
 * 消费方按优先级消费：config → concept → skill。
 */
export function resolveRoute(hits: RouteQueryResult[]): {
  configCandidates: RouteQueryResult[]
  conceptHit: RouteQueryResult | null
  skillHit: RouteQueryResult | null
} {
  const configCandidates = hits.filter(h => h.targetType === 'config')
  const conceptHit = hits.find(h => h.targetType === 'concept') || null
  const skillHit = hits.find(h => h.targetType === 'skill') || null

  return {
    configCandidates,
    conceptHit,
    skillHit,
  }
}

// ── 通用学习（D38-8-INV-01） ────────────────────────────────────────

/**
 * 统一学习入口：将用户确认的匹配写入 personalProfile。
 *
 * 支持三种目标类型（config / concept / skill），
 * 共享同一套 (freq, lastHit) 迭代机制。
 *
 * @param userInput 用户输入的原始短语
 * @param target 路由目标（类型 + ID）
 * @param layer 写入层（默认 personalProfile）
 */
export function confirmRoute(
  userInput: string,
  target: { targetType: TargetType; targetId: string },
  layer: 'personalProfile' | 'baseline' = 'personalProfile',
): void {
  const idx = loadIndex()
  const entryKey = `${userInput}::${target.targetType}::${target.targetId}`

  if (idx[layer][entryKey]) {
    const entry = idx[layer][entryKey]
    entry.freq++
    entry.lastHit = now()
  } else {
    idx[layer][entryKey] = makeEntry(userInput, target.targetId, target.targetType, layer === 'personalProfile' ? 'personalProfile' : 'baseline')
  }

  saveIndex()
}

// ── Confirm（旧接口 + 新概念接口） ───────────────────────────────────

/**
 * 用户确认某个 config 匹配（旧接口兼容层）
 * 内部调用 confirmRoute。
 */
export function confirmMatch(userInput: string, configKey: string): void {
  const idx = loadIndex()
  const entryKey = `${userInput}::config::${configKey}`

  // 检查是否已存在
  if (idx.personalProfile[entryKey]) {
    const entry = idx.personalProfile[entryKey]
    entry.freq++
    entry.lastHit = now()
  } else {
    idx.personalProfile[entryKey] = makeEntry(userInput, configKey, 'config', 'personalProfile')
  }

  // 同时增加 baseline 中对应的命中计数（如果有）
  for (const [bKey, bEntry] of Object.entries(idx.baseline)) {
    if (bEntry.targetType === 'config' && bEntry.targetId === configKey) {
      // 关联增加 baseline 的 freq，但不修改 lastHit（baseline 的权重本身低）
      bEntry.freq = Math.min(bEntry.freq + 1, 100)
    }
  }

  saveIndex()
}

/**
 * 用户确认某个 concept 匹配（概念短路学习）
 * 当用户选择主动反问提供的概念选项时调用。
 */
export function confirmConcept(userInput: string, conceptId: string): void {
  confirmRoute(userInput, { targetType: 'concept', targetId: conceptId })
}

/**
 * 记录一次命中（无论来自 baseline 还是 personalProfile）
 */
export function recordHit(userInput: string, configKey: string): void {
  const idx = loadIndex()
  const lowerInput = userInput.toLowerCase().trim()

  // 更新 personalProfile
  for (const [, entry] of Object.entries(idx.personalProfile)) {
    if (entry.targetType === 'config' && entry.targetId === configKey &&
        (entry.keyword.toLowerCase() === lowerInput ||
         entry.keyword.toLowerCase().includes(lowerInput))) {
      entry.freq++
      entry.lastHit = now()
      saveIndex()
      return
    }
  }

  // 更新 baseline
  for (const [, entry] of Object.entries(idx.baseline)) {
    if (entry.targetType === 'config' && entry.targetId === configKey &&
        (entry.keyword.toLowerCase() === lowerInput ||
         entry.keyword.toLowerCase().includes(lowerInput))) {
      entry.freq++
      entry.lastHit = now()
      saveIndex()
      return
    }
  }
}

// ── D38-8-方案6：概念级种子关键词 ──────────────────────────────────

/**
 * 初始化种子概念关键词（D38-8-INV-05）。
 *
 * 将 bizKnowledge 中的 boundaryKeywords 写入 baseline 层，
 * 使得 queryAllLayers / resolveRoute 在冷启动时即可识别概念请求。
 * 种子概念的 freq 初始为 0，触发主动反问流程。
 * 用户在反问中选择确认后 → confirmConcept → freq≥1 → 短路。
 *
 * @param keywords keyword → conceptId 对
 */
export function seedConceptKeywords(
  keywords: Array<{ keyword: string; conceptId: string }>
): void {
  const idx = loadIndex()
  let changed = false
  for (const { keyword, conceptId } of keywords) {
    const entryKey = `${keyword}::concept::${conceptId}`
    if (!idx.baseline[entryKey]) {
      idx.baseline[entryKey] = makeEntry(keyword, conceptId, 'concept', 'baseline')
      changed = true
    }
  }
  if (changed) saveIndex()
}

// ── Export for diagnostics ────────────────────────────────────────────

export function getIndexStats(): {
  baselineCount: number
  personalCount: number
  groupCount: number
  topPersonal: Array<{ keyword: string; targetId: string; freq: number }>
} {
  const idx = loadIndex()
  const topPersonal = Object.values(idx.personalProfile)
    .sort((a, b) => b.freq - a.freq)
    .slice(0, 10)
    .map(e => ({ keyword: e.keyword, targetId: e.targetId, freq: e.freq }))

  return {
    baselineCount: Object.keys(idx.baseline).length,
    personalCount: Object.keys(idx.personalProfile).length,
    groupCount: Object.keys(idx.groupProfile).length,
    topPersonal,
  }
}

/**
 * 构建 configKey → 业务标签 映射表。
 * 用于 list_configs 等工具结果格式化，将 key 翻译为用户友好的中文名。
 */
export function getConfigLabelMap(): Record<string, string> {
  const map: Record<string, string> = {}
  // 从 BASELINE_KW 取每个 configKey 的第一个 keyword 作为标签
  for (const [keyword, keys] of Object.entries(BASELINE_KW)) {
    for (const key of keys) {
      if (!map[key]) map[key] = keyword
    }
  }
  return map
}
