/**
 * ask_user — ASK 工具
 *
 * D38-4-INV-04: ASK 工具化 + 管线挂起
 * 当 L1 未匹配 subject 时，ASK 工具作为普通 planStep 加入管线。
 * ASK 不执行 IPC，挂起管线等待用户确认别名后恢复执行。
 */

import type { FunctionTool, ToolResult } from '../types'

export const askUserTool: FunctionTool = {
  definition: {
    name: 'ask_user',
    description: '向用户提问并等待确认。用于 L1 在配置索引中未找到用户目标时，询问用户并注册别名。',
    parameters: {
      type: 'object',
      properties: {
        question: {
          type: 'string',
          description: '向用户展示的问题',
        },
        candidates: {
          type: 'array',
          items: { type: 'string' },
          description: '候选答案列表（配置项的中文名）',
        },
        original_subject: {
          type: 'string',
          description: '用户原始 subject，确认后注册别名',
        },
        fallback_message: {
          type: 'string',
          description: '用户否认时的友好退出消息',
        },
      },
      required: ['question', 'candidates', 'original_subject', 'fallback_message'],
    },
  },

  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ ASK 执行失败']
    return [`❓ ${String(d.question || '请确认')}`]
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    // ASK 工具不做任何 IPC 调用
    // 执行器检测到 ask_user 工具名时，将管线挂起（ctx.suspend()）
    // 此处只返回标记，由管线层处理挂起逻辑
    return {
      success: true,
      data: {
        awaiting: true,
        question: String(args.question || ''),
        candidates: args.candidates as string[],
        originalSubject: String(args.original_subject || ''),
        fallbackMessage: String(args.fallback_message || ''),
      },
    }
  },
}
