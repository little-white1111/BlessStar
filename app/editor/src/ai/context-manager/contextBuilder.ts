import type { AIMessage, FunctionToolParam } from '../types'

// ── Types ──────────────────────────────────────────────────────────────

export interface CompactIndex {
  fieldSemantics: string     // field_semantics.compact 全部内容
  domainKnowledge: string    // domain_knowledge.compact 全部内容
  constraintKnowledge: string // constraint_knowledge.compact 全部内容
}

export interface ToolDelta {
  summary: string  // 单行摘要，如 "✅ 已创建字段: host_address (string, required, widget=input)"
}

export interface ContextBuilderInput {
  userInput: string                  // 当前用户输入
  systemPrompt: string               // system prompt（角色+工具选择指令）
  toolDefs: FunctionToolParam[]      // 5 个 function tool 定义
  indexCompact: CompactIndex | null  // compact 索引（从预生成文件读取）
  lastToolDelta?: ToolDelta          // 可选：上一轮 tool 结果摘要
  /**
   * 可选：历史对话消息（最近的对话轮次）。
   * 按时间顺序排列，在 system 之后、当前 user 之前插入。
   * 建议保留最近 3-5 轮助手的工具调用 + 用户反馈。
   * 只传 assistant 和 user 角色，不传 system。
   */
  historyMessages?: AIMessage[]
}

const INDEX_SEPARATOR = '=== Agent Skill Index ==='

/**
 * 构建上下文，每次请求固定 messages 长度。
 *
 * Layer 3: system prompt + tool defs → 工具契约
 * Layer 2: compact index → 业务知识库
 * Layer 1: 用户输入 + tool delta → 工作记忆
 */
export function buildContext(input: ContextBuilderInput): AIMessage[] {
  const result: AIMessage[] = []

  // ── 构建 system message (Layer 3 + Layer 2) ──
  let systemContent = input.systemPrompt

  // 附加 tool 定义到 system message
  if (input.toolDefs && input.toolDefs.length > 0) {
    systemContent += '\n\n## 可用工具\n'
    systemContent += input.toolDefs
      .map((t) => {
        const props = t.parameters?.properties
          ? Object.keys(t.parameters.properties as Record<string, unknown>).join(', ')
          : ''
        return `- ${t.name}: ${t.description}（参数: ${props}）`
      })
      .join('\n')
  }

  // 注入 compact 索引 (Layer 2)
  if (input.indexCompact) {
    const compactParts: string[] = []
    if (input.indexCompact.fieldSemantics) {
      compactParts.push(input.indexCompact.fieldSemantics)
    }
    if (input.indexCompact.domainKnowledge) {
      compactParts.push(input.indexCompact.domainKnowledge)
    }
    if (input.indexCompact.constraintKnowledge) {
      compactParts.push(input.indexCompact.constraintKnowledge)
    }
    if (compactParts.length > 0) {
      systemContent += `\n\n${INDEX_SEPARATOR}\n${compactParts.join('\n')}`
    }
  }

  result.push({ role: 'system', content: systemContent })

  // ── 可选：注入历史对话消息 ──
  if (input.historyMessages && input.historyMessages.length > 0) {
    // 限制最多传 5 轮（10 条消息），避免 token 过长
    const recent = input.historyMessages.slice(-10)
    for (const msg of recent) {
      if (msg.role === 'system') continue // 跳过 system
      // DeepSeek/OpenAI 要求 tool 消息必须带 tool_call_id，没有则降级为 user
      if (msg.role === 'tool') {
        result.push({
          role: msg.tool_call_id ? 'tool' : 'user',
          content: msg.content || '',
        })
      } else {
        result.push({
          role: msg.role,
          content: msg.content || '',
        })
      }
    }
  }

  // ── 可选 tool delta (Layer 1 work memory) ──
  if (input.lastToolDelta) {
    // 用 user 角色而非 tool 角色，避免 DeepSeek/OpenAI 要求 tool_call_id
    result.push({
      role: 'user',
      content: `[上轮工具结果] ${input.lastToolDelta.summary}`,
    })
  }

  // ── 用户输入 (Layer 1 work memory) ──
  result.push({ role: 'user', content: input.userInput })

  return result
}
