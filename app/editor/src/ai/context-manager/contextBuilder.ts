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

  // ── 可选 tool delta (Layer 1 work memory) ──
  if (input.lastToolDelta) {
    result.push({
      role: 'tool',
      content: input.lastToolDelta.summary,
    })
  }

  // ── 用户输入 (Layer 1 work memory) ──
  result.push({ role: 'user', content: input.userInput })

  return result
}
