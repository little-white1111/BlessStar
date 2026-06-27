/**
 * chat — 兜底对话工具
 *
 * 当用户带有询问语气且其他所有 tool 都无法满足需求时调用。
 * 最高原则：不执行任何系统操作，仅返回 LLM 的直接回复文本。
 *
 * 使用场景：
 *   - 概念解释："什么是 Schema？"
 *   - 建议咨询："我应该怎么配置弹幕？"
 *   - 无法匹配任何配置/文件/规则操作的闲聊或问答
 */

import type { ToolResult } from '../types'
import { createTool } from './toolFactory'

export const chatTool = createTool({
  name: 'chat',
  description: '兜底对话工具。当用户询问语气且其他工具都不满足需求时调用。仅返回 LLM 文本回复，不执行任何系统操作。',
  category: 'execution',
  params: {
    reply: {
      type: 'string',
      description: 'LLM 对用户的直接回复文本。应基于已有知识进行准确回复，不要编造配置或操作。',
      required: true,
    },
  },

  resultSchema: {
    fields: [
      { name: 'reply', type: 'string', label: '回复', priority: 1 },
    ],
    successTemplate: '{reply}',
    errorTemplate: '❌ chat: {error}',
  },

  /** 自定义渲染：直接展示回复文本 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d?.reply) return ['（无回复内容）']
    return [String(d.reply)]
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const reply = String(args.reply || '')
    return {
      success: true,
      data: { reply },
    }
  },
})
