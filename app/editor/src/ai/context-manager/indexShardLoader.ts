/**
 * indexShardLoader — Compact Index 分片加载
 *
 * 对应 GAP-16（Compact Index 分片注入——只注入意图匹配的 domain 片段）。
 * 参考 DeepSeek V4 CSA 的稀疏注意力思想，只加载最相关的索引片段。
 *
 * 当前：全量 compact index 一次性注入（~1200 tokens）
 * 改进：按意图关键词过滤 domain 片段，平均减 50-60% context token 占用量
 *
 * 三层压缩策略：
 * ① Tool Router：15→1 个 tool definition（已由 toolRouter.ts 实现）
 * ② Compact Index 分片加载（本文件）
 * ③ Execution Trace 分层注入（由 executionTrace.ts 实现）
 */

import type { CompactIndex } from './contextBuilder'
import { BusinessAdapterRegistry } from '../business-adapter/registry'

// ── Domain 索引 ──────────────────────────────────────────────────────

/**
 * Domain 定义：一个业务领域对应的索引片段
 */
export interface DomainShard {
  domainName: string
  /** 匹配关键词（意图中包含这些词时激活此 domain） */
  keywords: string[]
  /** 领域知识描述 */
  domainDescription: string
  /** 相关字段语义（field_semantics.compact 片段） */
  fieldSemantics?: string
  /** 相关约束知识（constraint_knowledge.compact 片段） */
  constraintKnowledge?: string
}

/**
 * 内置 Domain 索引表。
 * 在厂商配置解析（T5）阶段，此表由 bs_config_declare() 自动填充。
 * MVP 阶段为静态定义，后续变为可注册。
 */
const BUILTIN_DOMAINS: DomainShard[] = [
  {
    domainName: 'connection',
    keywords: ['连接', 'connection', 'connect', 'host', 'port', '地址', '端口', 'hostname'],
    domainDescription: '连接配置：主机、端口、超时、连接池',
    fieldSemantics: 'connection: host/port/timeout/pool',
    constraintKnowledge: 'host required; port range 1-65535; timeout >= 1000',
  },
  {
    domainName: 'security',
    keywords: ['安全', 'security', 'ssl', 'tls', '认证', 'auth', '密码', 'password', '密钥', 'key'],
    domainDescription: '安全配置：SSL/TLS、认证、密钥、密码',
    fieldSemantics: 'security: ssl_enabled/cert_path/key_path/auth_type',
    constraintKnowledge: 'ssl_enabled bool; cert_path required if ssl_enabled',
  },
  {
    domainName: 'logging',
    keywords: ['日志', 'log', 'logging', 'log_level', 'trace', 'debug'],
    domainDescription: '日志配置：日志级别、输出路径、格式',
    fieldSemantics: 'logging: log_level/log_path/log_format/max_size',
    constraintKnowledge: 'log_level in (debug,info,warn,error); max_size <= 100MB',
  },
  {
    domainName: 'room',
    keywords: ['房间', 'room', 'live', '直播', 'live', 'danmaku', '弹幕', 'gift', '礼物'],
    domainDescription: '直播房间配置：房间号、弹幕设置、礼物配置',
    fieldSemantics: 'room: room_id/room_name/announcement/danmaku_settings/gift_settings',
    constraintKnowledge: 'room_id required; danmaku_settings object',
  },
  {
    domainName: 'payment',
    keywords: ['支付', 'payment', '金额', 'amount', 'price', '价格', '收费', 'fee'],
    domainDescription: '支付配置：金额、费率、支付渠道',
    fieldSemantics: 'payment: amount/currency/fee_rate/channel/min_amount/max_amount',
    constraintKnowledge: 'fee_rate 0-1; min_amount >= 0; max_amount > min_amount',
  },
  {
    domainName: 'database',
    keywords: ['数据库', 'database', 'db', 'mysql', 'redis', 'sql', '存储', 'storage'],
    domainDescription: '数据库配置：连接字符串、连接池、超时',
    fieldSemantics: 'database: db_type/db_host/db_port/db_name/pool_size/timeout',
    constraintKnowledge: 'db_type in (mysql,postgresql,redis); pool_size >= 1',
  },
  {
    domainName: 'models',
    keywords: ['模型', 'models', 'model', 'template', '模板', '配置', 'config'],
    domainDescription: '模型/配置文件管理',
    fieldSemantics: 'models: model_name/version/file_path/format',
    constraintKnowledge: 'model_name required; format in (json,yaml,toml)',
  },
  {
    domainName: 'gate',
    keywords: ['规则', 'gate', 'rule', '校验', 'validate', '约束', 'constraint'],
    domainDescription: 'Gate 规则配置：校验规则、条件、表达式',
    fieldSemantics: 'gate: gate_id/gate_type/condition/expression/action',
    constraintKnowledge: 'gate_id unique; expression valid syntax',
  },

  // ── LiveDesign 业务领域 ──
  // 由 BusinessAdapterRegistry 在启动时注入
]

/* ══════════════════════════════════════════════════════════════════
 * 动态 Domain 推导（E2E-05 Agent 自动重建）
 *
 * 从 bs_config_declare() 已注册的 Schema 字段中，
 * 按 2 层 key 前缀自动生成 DomainShard。
 * 若 Schema 不可用，回退至上方 BUILTIN_DOMAINS 静态表。
 * ══════════════════════════════════════════════════════════════════ */

/** 缓存的动态 domains（initDynamicDomains() 填充） */
let cachedDynamicDomains: DomainShard[] | null = null

/**
 * 从 Schema JSON 对象推导 domain 分片列表。
 * 每个 2 级 key 前缀（如 "livedesign.room"）对应一个 domain，
 * 该 domain 的 keywords 从前缀名称和字段描述中提取。
 */
function deriveDomainsFromSchema(schema: any): DomainShard[] {
  if (!schema || !schema.fields || !Array.isArray(schema.fields)) {
    return []
  }

  const fields = schema.fields as Array<{ key: string; type_name?: string; description?: string; default_value?: string }>
  const groupMap = new Map<string, { keys: string[]; descriptions: string[]; fields: string }>()

  for (const f of fields) {
    if (!f.key) continue
    const parts = f.key.split('.')
    const prefix = parts.length >= 3 ? parts.slice(0, 2).join('.') : parts[0]

    if (!groupMap.has(prefix)) {
      groupMap.set(prefix, { keys: [], descriptions: [], fields: '' })
    }
    const entry = groupMap.get(prefix)!
    entry.keys.push(f.key)
    if (f.description) entry.descriptions.push(f.description)
  }

  const domains: DomainShard[] = []
  for (const [prefix, entry] of groupMap.entries()) {
    const keywords = [
      prefix,
      ...prefix.split('.'),
      ...entry.descriptions.flatMap((d) => d.split(/\s+/)),
    ]
    // Deduplicate keywords
    const uniqueKeywords = [...new Set(keywords.filter((k) => k.length > 1))]

    const domainName = prefix.replace(/\./g, '-')
    const fieldSemantics = entry.keys.join(', ')
    const domainDescription = `${prefix} 配置组：${entry.descriptions.join('；') || '无描述'}`

    domains.push({
      domainName,
      keywords: uniqueKeywords,
      domainDescription,
      fieldSemantics,
      constraintKnowledge: `${prefix} 字段数：${entry.keys.length}`,
    })
  }

  return domains
}

/**
 * initDynamicDomains — 从已注册 Schema 加载动态 domain 索引
 *
 * 在 Editor 初始化时调用（如 app.whenReady 或 Editor 组件 mount 后）。
 * 若 Schema 不可用，cachedDynamicDomains 保持 null，后续回退至静态表。
 */
export async function initDynamicDomains(): Promise<void> {
  try {
    const schema = await window.blessstar.getRegisteredSchemas()
    const dynamic = deriveDomainsFromSchema(schema)
    if (dynamic.length > 0) {
      cachedDynamicDomains = dynamic
      console.log(`[indexShardLoader] 动态加载 ${dynamic.length} 个 domain（来自 Schema）`)
    }
  } catch (err) {
    console.warn('[indexShardLoader] 动态 domain 加载失败，使用静态表:', err)
  }
}

/**
 * getAllDomains — 获取合并后的 domain 列表（动态优先，静态回退 + Registry 业务分片追加）
 */
export function getAllDomains(): DomainShard[] {
  const base = cachedDynamicDomains && cachedDynamicDomains.length > 0
    ? cachedDynamicDomains
    : [...BUILTIN_DOMAINS]

  // 合并 Registry 中的业务 domain shard（由适配器注入）
  const registryData = BusinessAdapterRegistry.getMergedAIData()
  if (registryData.domainShards && registryData.domainShards.length > 0) {
    for (const shard of registryData.domainShards) {
      const existing = base.findIndex(d => d.domainName === shard.domainName)
      if (existing >= 0) {
        base[existing] = shard  // 覆盖同名
      } else {
        base.push(shard)        // 追加新增
      }
    }
  }

  return base
}

// ── P2: Domain 语义匹配 ─────────────────────────────────────────────

/**
 * P2 + EMB: 双层语义匹配。
 *
 * 第一层：Jaccard 重叠加权（~0ms，覆盖 ~80% 场景）
 * 第二层：当 Jaccard < 0.15 时，尝试 embedding 余弦相似度二次精排（+~200ms 首次 / ~0ms 缓存后）
 *
 * 嵌入向量由 initEmbeddingCache() 预计算并缓存，embedding 不可用时回退到关键字匹配。
 */
export async function selectIndexShardsSemantic(
  intent: string,
  domains: DomainShard[],
  jaccardThreshold = 0.15,
  cosineThreshold = 0.5,
): Promise<ShardLoadResult> {
  if (!intent || intent.trim().length === 0) {
    return {
      domainKnowledge: '',
      fieldSemantics: '',
      constraintKnowledge: '',
      matchedCount: 0,
      totalDomains: domains.length,
      compressionRatio: 1.0,
    }
  }

  // ── 第一层：Jaccard 重叠加权 ──
  const queryTokens = tokenize(intent)
  if (queryTokens.size === 0) {
    return selectIndexShards(intent, domains)
  }

  const jaccardScores: Array<{ domain: DomainShard; score: number }> = domains.map(d => {
    const domainText = [
      ...d.keywords,
      d.domainDescription,
      d.fieldSemantics || '',
      d.constraintKnowledge || '',
    ].join(' ')
    const domainTokens = tokenize(domainText)
    if (domainTokens.size === 0) return { domain: d, score: 0 }
    const intersection = new Set([...queryTokens].filter(t => domainTokens.has(t)))
    const union = new Set([...queryTokens, ...domainTokens])
    return { domain: d, score: intersection.size / union.size }
  })

  const jaccardMatched = jaccardScores
    .filter(s => s.score >= jaccardThreshold)
    .sort((a, b) => b.score - a.score)

  // Jaccard 有命中 → 直接用 Jaccard 结果
  if (jaccardMatched.length > 0) {
    return buildShardResult(jaccardMatched.map(s => s.domain), domains)
  }

  // ── 第二层：Jaccard < 阈值 → 尝试 embedding 余弦精排 ──
  const cosineScores = await tryEmbeddingRank(intent, domains)
  if (cosineScores) {
    const scored: Array<{ domain: DomainShard; score: number }> = domains.map((d, i) => ({
      domain: d,
      score: cosineScores[i],
    }))
    const cosineMatched = scored
      .filter(s => s.score >= cosineThreshold)
      .sort((a, b) => b.score - a.score)

    if (cosineMatched.length > 0) {
      return buildShardResult(cosineMatched.map(s => s.domain), domains)
    }
  }

  // 两层均未命中 → 回退到关键字匹配
  return selectIndexShards(intent, domains)
}

/**
 * 构建 ShardLoadResult（公共逻辑）。
 */
function buildShardResult(matchedDomains: DomainShard[], allDomains: DomainShard[]): ShardLoadResult {
  const totalPotentialSize = allDomains.reduce(
    (sum, d) => sum + d.domainDescription.length + (d.fieldSemantics?.length || 0) + (d.constraintKnowledge?.length || 0),
    0,
  )
  const loadedSize = matchedDomains.reduce(
    (sum, d) => sum + d.domainDescription.length + (d.fieldSemantics?.length || 0) + (d.constraintKnowledge?.length || 0),
    0,
  )
  return {
    domainKnowledge: matchedDomains.map(d => d.domainDescription).join('\n'),
    fieldSemantics: matchedDomains.map(d => d.fieldSemantics || '').filter(Boolean).join('\n'),
    constraintKnowledge: matchedDomains.map(d => d.constraintKnowledge || '').filter(Boolean).join('\n'),
    matchedCount: matchedDomains.length,
    totalDomains: allDomains.length,
    compressionRatio: totalPotentialSize > 0 ? 1 - (loadedSize / totalPotentialSize) : 1,
  }
}

/**
 * 将中文/英文文本分词为中文字符 + 英文词片段。
 * 中文按单字切分（无分词库时的轻量方案），英文按空白/标点切分。
 */
function tokenize(text: string): Set<string> {
  const tokens = new Set<string>()

  // 提取英文字母数字词
  const enTokens = text.toLowerCase().match(/[a-z][a-z0-9]*/g)
  if (enTokens) {
    for (const t of enTokens) {
      if (t.length >= 2) tokens.add(t) // 跳过单字母
    }
  }

  // 提取中文字符双字组合（无分词库时的近似）
  const chChars = text.match(/[\u4e00-\u9fff]/g)
  if (chChars) {
    // 双字组（bigram）作为 token — 对中文语义匹配更敏感
    for (let i = 0; i < chChars.length - 1; i++) {
      tokens.add(chChars[i] + chChars[i + 1])
    }
    // 也加入单字（以与部分关键词匹配）
    for (let i = 0; i < chChars.length; i++) {
      tokens.add(chChars[i])
    }
  }

  return tokens
}

// ── EMB: Embedding 缓存管理 ──────────────────────────────────────────

/**
 * EMB: 预计算的 domain embedding 向量缓存。
 * key = domainName, value = number[]（embedding 向量）
 */
let domainEmbeddingCache: Map<string, number[]> | null = null

/**
 * EMB: embedding 函数引用（由 initEmbeddingCache 注入）。
 * 类型：async (text: string) => number[]
 */
let embedFunction: ((text: string) => Promise<number[]>) | null = null

/**
 * EMB: 初始化 embedding 缓存。
 * 在 Editor 启动、模型切换、schema 变更时调用。
 * 并行预计算所有 domain 的 embedding 向量，存入内存缓存。
 *
 * @param embedFn - AIBridge.embed() 函数引用
 * @param domains - 所有 DomainShard（内置 + 动态）
 */
export async function initEmbeddingCache(
  embedFn: (text: string) => Promise<number[]>,
  domains: DomainShard[],
): Promise<void> {
  embedFunction = embedFn
  const cache = new Map<string, number[]>()

  // 并行预计算所有 domain embedding
  await Promise.all(domains.map(async (d) => {
    const domainText = [
      ...d.keywords,
      d.domainDescription,
      d.fieldSemantics || '',
      d.constraintKnowledge || '',
    ].join(' ')
    if (!domainText.trim()) {
      cache.set(d.domainName, [])
      return
    }
    try {
      const vec = await embedFn(domainText)
      cache.set(d.domainName, vec)
    } catch {
      cache.set(d.domainName, [])
    }
  }))

  domainEmbeddingCache = cache
}

/**
 * EMB: 清空 embedding 缓存（模型切换时调用）。
 */
export function clearEmbeddingCache(): void {
  domainEmbeddingCache = null
  embedFunction = null
}

/**
 * EMB: 计算余弦相似度。
 */
function cosineSimilarity(a: number[], b: number[]): number {
  if (a.length === 0 || b.length === 0 || a.length !== b.length) return 0
  let dot = 0, na = 0, nb = 0
  for (let i = 0; i < a.length; i++) {
    dot += a[i] * b[i]
    na += a[i] * a[i]
    nb += b[i] * b[i]
  }
  const denom = Math.sqrt(na) * Math.sqrt(nb)
  return denom === 0 ? 0 : dot / denom
}

/**
 * EMB: 对 Jaccard < 0.15 的 query 尝试 embedding 余弦二次精排。
 *
 * @returns cosine 相似度数组，与 domains 一一对应。
 *          如果缓存不可用或全部失败，返回 null。
 */
async function tryEmbeddingRank(
  intent: string,
  domains: DomainShard[],
): Promise<number[] | null> {
  if (!domainEmbeddingCache || !embedFunction) return null

  // 对用户输入做一次 embedding
  let queryVec: number[]
  try {
    queryVec = await embedFunction(intent)
  } catch {
    return null
  }
  if (!queryVec || queryVec.length === 0) return null

  return domains.map(d => {
    const domainVec = domainEmbeddingCache!.get(d.domainName)
    if (!domainVec || domainVec.length === 0) return 0
    return cosineSimilarity(queryVec, domainVec)
  })
}

export interface ShardLoadResult {
  /** 匹配到的 Domain 描述（拼接后） */
  domainKnowledge: string
  /** 匹配到的字段语义（拼接后） */
  fieldSemantics: string
  /** 匹配到的约束知识（拼接后） */
  constraintKnowledge: string
  /** 匹配的 domain 数 */
  matchedCount: number
  /** 总 domain 数 */
  totalDomains: number
  /** 压缩率（1 - loadedSize/totalSize） */
  compressionRatio: number
}

/**
 * 根据意图选择最相关的 Domain 分片。
 *
 * 策略（V4 CSA 启发：top-k 稀疏选择）：
 * 1. 计算每个 domain 与意图的关键词匹配度
 * 2. 取匹配度最高的 domain（任意关键词匹配即选中）
 * 3. 如果没有任何 domain 匹配，返回空（不注入 index，省全部 token）
 *
 * @param intent 用户意图
 * @param domains 可选的 domain 列表（默认使用内置表）
 */
export function selectIndexShards(
  intent: string,
  domains: DomainShard[] = getAllDomains(),
): ShardLoadResult {
  if (!intent || intent.trim().length === 0) {
    return {
      domainKnowledge: '',
      fieldSemantics: '',
      constraintKnowledge: '',
      matchedCount: 0,
      totalDomains: domains.length,
      compressionRatio: 1.0,
    }
  }

  const lowerIntent = intent.toLowerCase()
  const matchedDomains: DomainShard[] = []

  for (const domain of domains) {
    const isMatched = domain.keywords.some((kw) => lowerIntent.includes(kw.toLowerCase()))
    if (isMatched) {
      matchedDomains.push(domain)
    }
  }

  // 计算分片大小和压缩率
  const totalPotentialSize = domains.reduce(
    (sum, d) => sum + d.domainDescription.length + (d.fieldSemantics?.length || 0) + (d.constraintKnowledge?.length || 0),
    0,
  )

  const loadedSize = matchedDomains.reduce(
    (sum, d) => sum + d.domainDescription.length + (d.fieldSemantics?.length || 0) + (d.constraintKnowledge?.length || 0),
    0,
  )

  return {
    domainKnowledge: matchedDomains.map((d) => d.domainDescription).join('\n'),
    fieldSemantics: matchedDomains.map((d) => d.fieldSemantics || '').filter(Boolean).join('\n'),
    constraintKnowledge: matchedDomains.map((d) => d.constraintKnowledge || '').filter(Boolean).join('\n'),
    matchedCount: matchedDomains.length,
    totalDomains: domains.length,
    compressionRatio: totalPotentialSize > 0 ? 1 - (loadedSize / totalPotentialSize) : 1,
  }
}

/**
 * 注册自定义 Domain（在业务系统初始化时调用）
 * 注意：当 cachedDynamicDomains 存在时，注册到动态表；否则注册到静态表。
 */
export function registerDomain(domain: DomainShard): void {
  const target = cachedDynamicDomains || BUILTIN_DOMAINS
  const existing = target.findIndex((d) => d.domainName === domain.domainName)
  if (existing >= 0) {
    target[existing] = domain
  } else {
    target.push(domain)
  }
}

/**
 * 从匹配 Domains 的 fieldSemantics 中提取配置 key 2级前缀集合。
 *
 * fieldSemantics 格式示例：
 *   "livedesign.live2d: model_path(STR)/model_directory(STR)/..."
 * 提取出: ["livedesign.live2d"]
 *
 * 用于在 L1 检索后按 domain 过滤候选，实现"窄域 READ"。
 */
export function extractDomainKeyPrefixes(result: ShardLoadResult): string[] {
  if (!result.fieldSemantics) return []
  const prefixes = new Set<string>()
  const parts = result.fieldSemantics.split('|')
  for (const part of parts) {
    const match = part.match(/^[\w.]+/)
    if (match) {
      // 取 2 层前缀（如 livedesign.live2d）
      const segments = match[0].split('.')
      if (segments.length >= 2) {
        prefixes.add(segments.slice(0, 2).join('.'))
      } else {
        prefixes.add(match[0])
      }
    }
  }
  return [...prefixes]
}
export function shardResultToCompactIndex(result: ShardLoadResult): CompactIndex | null {
  if (result.matchedCount === 0) return null

  const parts: string[] = []
  if (result.domainKnowledge) parts.push(result.domainKnowledge)
  if (result.fieldSemantics) parts.push(result.fieldSemantics)
  if (result.constraintKnowledge) parts.push(result.constraintKnowledge)

  return {
    fieldSemantics: result.fieldSemantics,
    domainKnowledge: result.domainKnowledge,
    constraintKnowledge: result.constraintKnowledge,
  }
}
