/**
 * executorLabels — Executor key pattern 中文标签映射
 *
 * D38-4-INV-06: executor pattern 独立索引
 * 与 KEY_LABELS 平行，供 L1 做 subject→executor key pattern 匹配。
 * 当 configKey 匹配未命中时，L1 检查此表决定是否走 QUERY 路径。
 */

export const EXECUTOR_PATTERNS: Record<string, string> = {
  '消息': 'query.audience',
  '观众': 'query.audience',
  '弹幕': 'query.audience',
  '留言': 'query.audience',
  '礼物': 'query.gift',
}

/**
 * 根据 subject 匹配 executor key pattern。
 * 包含式匹配：subject 包含任意关键词则返回对应 pattern。
 * @returns pattern 字符串，未匹配返回 null
 */
export function matchExecutorPattern(subject: string): string | null {
  if (!subject) return null
  const lower = subject.toLowerCase()
  for (const [keyword, pattern] of Object.entries(EXECUTOR_PATTERNS)) {
    if (lower.includes(keyword.toLowerCase())) {
      return pattern
    }
  }
  return null
}
