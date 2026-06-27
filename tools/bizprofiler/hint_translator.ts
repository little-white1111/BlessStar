/**
 * hint_translator.ts — LLM ai_hint 翻译器（可选）
 *
 * 作用：补全缺失的 ai_hint。
 * 输入：字段 key → LLM 推断用户意图 → 输出 ai_hint 中文短句。
 * 调用时机：bs_config_declare() 注册后，ai_hint 为空时调用。
 *
 * MVP 实现：返回预设规则映射；完整版需调 LLM API。
 */

export interface HintRequest {
  fieldKey: string
  fieldType: string
  defaultValue?: string
  description?: string
}

export interface HintResult {
  fieldKey: string
  aiHint: string
  confidence: 'high' | 'medium' | 'low'
  source: 'rule' | 'llm'
}

// ── 规则映射表 ──────────────────────────────────────────────────────
// 基于字段 key 的关键词匹配规则

const RULE_HINTS: Array<{ pattern: RegExp; hint: string }> = [
  { pattern: /host|hostname|server/i,         hint: '主机地址' },
  { pattern: /port/i,                         hint: '端口号' },
  { pattern: /user|username/i,                hint: '用户名' },
  { pattern: /pass|password|secret/i,         hint: '密码' },
  { pattern: /db_name|database/i,             hint: '数据库名' },
  { pattern: /timeout/i,                      hint: '超时时间(秒)' },
  { pattern: /pool_min|min_pool|min_idle/i,   hint: '最小连接数' },
  { pattern: /pool_max|max_pool|max_active/i, hint: '最大连接数' },
  { pattern: /ssl/i,                          hint: '启用 SSL 加密' },
  { pattern: /charset|encoding/i,             hint: '字符集编码' },
  { pattern: /retry|max_retry/i,              hint: '最大重试次数' },
  { pattern: /rate|limit|qps/i,               hint: '速率限制(QPS)' },
  { pattern: /threshold/i,                    hint: '阈值' },
  { pattern: /interval/i,                     hint: '间隔时间(秒)' },
  { pattern: /batch|batch_size/i,             hint: '批量大小' },
  { pattern: /path|dir|directory/i,           hint: '路径' },
  { pattern: /url|endpoint/i,                 hint: '连接地址(URL)' },
  { pattern: /email/i,                        hint: '邮箱地址' },
  { pattern: /phone|mobile/i,                 hint: '手机号' },
  { pattern: /debug|verbose/i,                hint: '调试模式开关' },
  { pattern: /enabled|enable/i,               hint: '启用开关' },
  { pattern: /log/i,                          hint: '日志配置' },
  { pattern: /cache/i,                        hint: '缓存配置' },
  { pattern: /token/i,                        hint: '令牌/Token' },
]

/**
 * 根据字段 key 推断 ai_hint。
 * 优先走规则映射（confidence=high），无规则匹配返回 medium。
 */
export function suggestAiHint(request: HintRequest): HintResult {
  // 如果已有 description，直接用
  if (request.description && request.description.length > 0) {
    return {
      fieldKey: request.fieldKey,
      aiHint: request.description,
      confidence: 'high',
      source: 'rule',
    }
  }

  // 规则匹配
  for (const rule of RULE_HINTS) {
    if (rule.pattern.test(request.fieldKey)) {
      return {
        fieldKey: request.fieldKey,
        aiHint: rule.hint,
        confidence: 'high',
        source: 'rule',
      }
    }
  }

  // 类型推断
  const typeHints: Record<string, string> = {
    int32: '整数配置',
    int64: '整数配置',
    string: '字符串配置',
    double: '浮点数配置',
    bool: '开关配置',
  }
  const typeHint = typeHints[request.fieldType]
  if (typeHint) {
    return {
      fieldKey: request.fieldKey,
      aiHint: typeHint,
      confidence: 'medium',
      source: 'rule',
    }
  }

  // 兜底
  return {
    fieldKey: request.fieldKey,
    aiHint: '自定义配置项',
    confidence: 'low',
    source: 'rule',
  }
}

/**
 * 批量翻译。
 */
export function batchSuggestAiHints(requests: HintRequest[]): HintResult[] {
  return requests.map(suggestAiHint)
}
