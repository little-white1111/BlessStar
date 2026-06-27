/**
 * llmFieldSelector — C 路径：轻量 LLM 字段选择器
 *
 * C 路径（兜底路径）：B 路径检索结果不理想时，发送⼀个极简请求
 * 给 LLM，要求从字段名列表中选出与用户意图最相关的 Top-K 字段。
 * 请求体 ≈ 100 tokens，回应 ≈ 50 tokens，合计 < 200 tokens。
 */

export interface FieldSelectionRequest {
  /** 用户自然语言输入 */
  userInput: string
  /** 候选字段名列表（来自 B 路径的检索结果，或全部字段名） */
  candidateFields: string[]
  /** 最大返回数 */
  topK: number
}

export interface FieldSelectionResponse {
  selectedFields: string[]
  reasoning: string
}

/**
 * 构造 LLM 提示词（纯文本，< 150 tokens）。
 */
export function buildFieldSelectionPrompt(req: FieldSelectionRequest): string {
  const fieldList = req.candidateFields.slice(0, 50).join(', ')
  // 极简指令 + 候选列表，不含 system prompt
  return [
    `用户说: "${req.userInput}"`,
    `候选字段: [${fieldList}]`,
    `请从中选出与用户意图最相关的 ${req.topK} 个字段，只返回字段名，逗号分隔。`,
    `然后换行写一句简短理由（10 字内）。`,
  ].join('\n')
}

/**
 * 解析 LLM 回应为结构化结果。
 */
export function parseFieldSelectionResponse(raw: string): FieldSelectionResponse {
  const lines = raw.trim().split('\n')
  if (lines.length === 0) {
    return { selectedFields: [], reasoning: '无法解析 LLM 回应' }
  }

  // 首行：字段名列表（逗号分隔）
  const fieldLine = lines[0]
  const selectedFields = fieldLine
    .split(',')
    .map((f) => f.trim())
    .filter((f) => f.length > 0)

  // 次行及之后：理由
  const reasoning = lines.slice(1).join(' ').trim() || '无理由'

  return { selectedFields, reasoning }
}

/**
 * 模拟 C 路径调用（可选：用于单元测试或 MVP 无 LLM 时的 fallback）。
 * 不做实际 API 请求，基于简单关键词匹配模拟 LLM 选择。
 */
export function mockFieldSelection(req: FieldSelectionRequest): FieldSelectionResponse {
  const input = req.userInput.toLowerCase()
  const scored = req.candidateFields.map((field) => {
    const f = field.toLowerCase()
    let score = 0
    if (f.includes(input)) score += 3
    // 每个字符单独匹配
    for (const ch of input) {
      if (f.includes(ch)) score += 0.5
    }
    // 字段名中下划线分割的单词与输入匹配加分
    const parts = f.split(/[._-]/)
    for (const part of parts) {
      if (input.includes(part) || part.includes(input)) score += 2
    }
    return { field, score }
  })

  scored.sort((a, b) => b.score - a.score)
  const top = scored.slice(0, req.topK)
  return {
    selectedFields: top.map((s) => s.field),
    reasoning: `模拟选择：基于关键词匹配力度评分 (top=${req.topK})`,
  }
}
