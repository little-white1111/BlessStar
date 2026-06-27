/**
 * fieldRetriever — B 路径：倒排索引 + 模糊匹配检索 + AdaptiveIndex
 *
 * B 路径（快速路径）：先走倒排索引（含三阶权重 AdaptiveIndex）查找，无精确匹配时降级模糊匹配。
 * 不经过 LLM，纯确定性逻辑。
 *
 * 倒排索引数据由 BusinessAdapterRegistry 注入。
 * D38-5-INV-03: Registry 启动注入，运行时只读
 */

import { BusinessAdapterRegistry } from '../business-adapter/registry'

export interface InvertedIndex {
  [keyword: string]: string[]
}

/** 倒排索引匹配结果 */
export interface FieldScore {
  field: string
  score: number
}

// ── 加载索引 ─────────────────────────────────────────────────────────

export function loadInvertedIndex(): InvertedIndex {
  const data = BusinessAdapterRegistry.getMergedAIData()
  return data.invertedIndex || {}
}

/**
 * 从倒排索引中检索匹配字段（精确+包含匹配，最长优先）
 */
export function retrieveFields(index: InvertedIndex, intent: string, maxResults: number = 5): string[] {
  if (!intent) return []
  const lowerIntent = intent.toLowerCase().trim()
  const matched = new Map<string, number>()

  for (const [keyword, tools] of Object.entries(index)) {
    const lowerKw = keyword.toLowerCase()
    if (lowerKw === lowerIntent || lowerKw.includes(lowerIntent) || lowerIntent.includes(lowerKw)) {
      for (const tool of tools) {
        // score: 完全匹配最高，包含匹配次之
        const score = lowerKw === lowerIntent ? 100 : Math.max(lowerKw.length, 1)
        const existing = matched.get(tool) ?? 0
        matched.set(tool, existing + score)
      }
    }
  }

  return Array.from(matched.entries())
    .sort((a, b) => b[1] - a[1])
    .slice(0, maxResults)
    .map(([tool]) => tool)
}

/**
 * 带评分的检索（用于 Top-K 展示给 LLM）
 */
export function scoredRetrieve(index: InvertedIndex, intent: string, maxResults: number = 10): FieldScore[] {
  if (!intent) return []
  const lowerIntent = intent.toLowerCase().trim()
  const scored = new Map<string, number>()

  for (const [keyword, tools] of Object.entries(index)) {
    const lowerKw = keyword.toLowerCase()
    if (lowerKw === lowerIntent || lowerKw.includes(lowerIntent) || lowerIntent.includes(lowerKw)) {
      for (const tool of tools) {
        const score = lowerKw === lowerIntent ? 100 : Math.max(lowerKw.length, 1)
        const existing = scored.get(tool) ?? 0
        scored.set(tool, existing + score)
      }
    }
  }

  return Array.from(scored.entries())
    .sort((a, b) => b[1] - a[1])
    .slice(0, maxResults)
    .map(([field, score]) => ({ field, score }))
}

/** 触发完整重标定（重置缓存，下次请求重新构建） */
export function triggerRecalibration(): void {
  // 重置倒排索引缓存（如果有）
  // 自适应检索已移除（D38-FIX-ADAPTIVE-RETIRE）
}
