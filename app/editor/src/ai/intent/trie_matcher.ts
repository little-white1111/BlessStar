/**
 * trie_matcher.ts — 关键词三元组匹配器
 *
 * 接收用户句子，使用 trie_dict.ts 内的规则表进行关键词匹配。
 * 命中 → 直接输出 BlessStarIntent 三元组，绕过 LLM 意图识别。
 * 未命中 → 返回 null，降级由 LLM 做参数映射。
 *
 * 架构方案：④ 关键词三元组压缩（配置+操作+规则）
 * 见：架构方案选择记录（第25天以后）.md § 第32天/专题二
 */

import { findLongestKeyword, extractRuleFragment, OP_KW, DOMAIN_KW } from './trie_dict'
import type { BlessStarIntent } from './trie_dict'

/**
 * 尝试用关键词规则表压缩用户意图为三元组。
 * @param sentence 用户单条子句（Lexer 切分后的）
 * @returns BlessStarIntent | null（null = 降级给 LLM）
 */
export function compressIntent(sentence: string): BlessStarIntent | null {
  if (!sentence) return null

  /* 1. 查找操作 */
  const opMatch = findLongestKeyword(sentence, OP_KW)
  if (!opMatch) return null /* 无操作词 → 交给 LLM */

  /* 2. 查找领域 */
  const domainMatch = findLongestKeyword(sentence, DOMAIN_KW)
  if (!domainMatch) return null /* 无领域词 → 交给 LLM */

  /* 3. 提取规则 */
  const rule = extractRuleFragment(sentence)

  /* 4. 构造三元组 */
  const intent: BlessStarIntent = {
    config: { domain: domainMatch.value },
    operation: opMatch.value as BlessStarIntent['operation'],
  }

  if (rule) {
    intent.rule = rule
  }

  /* 从句子中提取直接值（若规则未提取到） */
  const valueMatch = sentence.match(/(\d[\d,.]*)/)
  if (!intent.rule && valueMatch) {
    intent.config.value = valueMatch[1].replace(/,/g, '')
  }

  return intent
}

/**
 * 批量压缩多句子（Lexer 输出 → 三元组数组）
 */
export function compressIntents(sentences: string[]): Array<{ sentence: string; intent: BlessStarIntent | null }> {
  return sentences.map((s) => ({
    sentence: s,
    intent: compressIntent(s),
  }))
}
