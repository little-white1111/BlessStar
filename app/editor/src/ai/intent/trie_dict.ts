/**
 * trie_dict.ts — 关键词三元组压缩字典
 *
 * 定义 BlessStarIntent 类型、OP_KW/DOMAIN_KW/RULE_KW 三张规则表。
 * 架构方案：关键词三元组（配置+操作+规则）压缩，减少 prompt tokens 72%。
 *
 * DOMAIN_KW 出厂为空，由 BusinessAdapterRegistry 注入。
 */

import { BusinessAdapterRegistry } from '../business-adapter/registry'

/** 匹配到的规则片段 */
export interface RuleFragment {
  op: string        /* "gt" | "gte" | "lt" | "lte" | "eq" | "ne" | "in" */
  value: string
}

/** 三元组意图结构 */
export interface BlessStarIntent {
  config: {
    domain: string
    field?: string
    value?: string
  }
  operation: 'read' | 'write' | 'gate' | 'schema' | 'search' | 'list'
  rule?: RuleFragment
}

/* ── 操作关键词表 ──────────────────────────────────────────────────────
 * 命中 → 确定 operation 字段
 */
export const OP_KW: Record<string, string> = {
  '改成':'write', '改为':'write', '设为':'write', '设置':'write',
  '写入':'write', '修改':'write', '更新':'write', '变更':'write',
  '禁用':'gate', '启用':'gate', '开启':'gate', '关闭':'gate',
  '限制':'gate', '校验':'gate', '检查':'gate', '审核':'gate',
  '删除':'schema', '移除':'schema', '新增':'schema', '添加':'schema',
  '查看':'read', '读取':'read', '显示':'list', '列出':'list',
  '搜索':'search', '查找':'search',
}

/* ── 领域关键词表 ──────────────────────────────────────────────────────
 * 启动时由 BusinessAdapterRegistry 注入业务领域词。
 * 出厂仅保留通用领域词。
 */
export const DOMAIN_KW: Record<string, string> = {
  '字段':'schema.field',
}

/** 从 BusinessAdapterRegistry 同步领域词 */
export function syncDomainKWFromRegistry(): void {
  const data = BusinessAdapterRegistry.getMergedAIData()
  if (data.trieDict?.domainKW) {
    Object.assign(DOMAIN_KW, data.trieDict.domainKW)
  }
}

if (BusinessAdapterRegistry.initialized) {
  syncDomainKWFromRegistry()
}

/* ── 操作符关键词表 ────────────────────────────────────────────────────
 * 从用户的口语表述提取 rule.op
 */
export const OP_MAP: Record<string, string> = {
  '大于': 'gt', '超过': 'gt', '高于': 'gt', '>': 'gt',
  '小于': 'lt', '低于': 'lt', '<': 'lt',
  '等于': 'eq', '=': 'eq', '==': 'eq',
  '不等于': 'ne', '!=': 'ne',
  '大于等于': 'gte', '≥': 'gte', '>=': 'gte',
  '小于等于': 'lte', '≤': 'lte', '<=': 'lte',
  '包含': 'in', '属于': 'in',
  '不包含': 'not_in', '不属于': 'not_in',
}

/** 查找用户句子中的关键词（最长匹配优先） */
export function findLongestKeyword(sentence: string, table: Record<string, string>): { key: string; value: string } | null {
  let best: { key: string; value: string } | null = null
  for (const kw of Object.keys(table)) {
    if (sentence.includes(kw)) {
      if (!best || kw.length > best.key.length) {
        best = { key: kw, value: table[kw] }
      }
    }
  }
  return best
}

/** 从句子中提取规则的操作符和数值 */
export function extractRuleFragment(sentence: string): RuleFragment | undefined {
  for (const [phrase, op] of Object.entries(OP_MAP)) {
    const idx = sentence.indexOf(phrase)
    if (idx === -1) continue
    /* 在操作符前后找数字 */
    const before = sentence.substring(0, idx)
    const after = sentence.substring(idx + phrase.length)
    const numMatch = (before.match(/(\d[\d,.]*)\s*$/) || after.match(/^\s*(\d[\d,.]*)/))
    if (numMatch) {
      const raw = numMatch[1].replace(/,/g, '')
      return { op, value: raw }
    }
  }
  /* 纯数字规则："禁用10级" */
  const numOnly = sentence.match(/(\d[\d,.]*)/)
  if (numOnly) {
    /* 判断语境：包含"以上"/"以下" */
    if (sentence.includes('以上') || sentence.includes('超过')) return { op: 'gte', value: numOnly[1].replace(/,/g, '') }
    if (sentence.includes('以下') || sentence.includes('低于')) return { op: 'lt', value: numOnly[1].replace(/,/g, '') }
    return { op: 'eq', value: numOnly[1].replace(/,/g, '') }
  }
  return undefined
}
