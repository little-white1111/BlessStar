/**
 * toolMatcher — 意图→工具精确查询管线
 *
 * 对应缺口一（AI-06）的 BlessStar-native 方案：AI 不生成 JSON 工具调用，
 * 只输出自然语言意图，系统通过工具倒排索引精确匹配到具体工具。
 *
 * 与 fieldRetriever.ts（CTX-04 B 路径）同构，复用同样的检索模式。
 */

import { loadInvertedIndex, retrieveFields, scoredRetrieve } from '../context-manager/fieldRetriever'
import type { InvertedIndex } from '../context-manager/fieldRetriever'

export interface ToolMatch {
  /** B 路径：找到了精确匹配的工具 */
  tools: string[]
  /** C 路径（LLM 兜底）：未匹配到任何工具，保留原始意图 */
  intent?: string
}

let _toolIndex: InvertedIndex | null = null

/**
 * 加载工具倒排索引（加载一次，后续复用）
 */
export function loadToolIndex(): InvertedIndex {
  if (_toolIndex) return _toolIndex
  // 从 JSON 资源加载，通过 IPC 或嵌入式方式
  // 此处结构化定义与 fieldRetriever.ts 共用 loadInvertedIndex 模式
  _toolIndex = loadInvertedIndex()
  return _toolIndex
}

/**
 * 手动注入工具索引（用于单元测试或 IPC 加载后注入）
 */
export function setToolIndex(index: InvertedIndex): void {
  _toolIndex = index
}

/**
 * 匹配工具：给定用户意图，返回匹配的工具名列表。
 *
 * 策略：
 * 1. B 路径：倒排索引精确/模糊匹配 → 返回对应的工具名
 * 2. C 路径（LLM 兜底）：若 B 路径无结果，返回原始意图由 LLM 兜底
 *
 * 匹配结果自动决定后续流程：
 * - B 路径命中 → 不走 AI 生成工具调用，系统自动填充参数
 * - C 路径兜底 → AI 降级到紧凑格式输出缺失的参数行
 */
export function matchTools(intent: string): ToolMatch {
  const index = _toolIndex || {}
  const matched = retrieveFields(index, intent, 5)

  if (matched.length > 0) {
    return { tools: matched }
  }

  // B 路径未命中 → C 路径 LLM 兜底
  return { tools: [], intent }
}

/**
 * 带评分的工具匹配（用于 Top-3 展示给用户/LLM 选择）
 */
export function scoredMatchTools(
  intent: string,
  maxResults: number = 3,
): Array<{ tool: string; score: number }> {
  const index = _toolIndex || {}
  const scored = scoredRetrieve(index, intent, maxResults)
  return scored.map((s) => ({ tool: s.field, score: s.score }))
}
