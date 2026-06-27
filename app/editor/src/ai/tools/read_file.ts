/**
 * read_file — 读取指定文件的内容（文本文件，UTF-8 编码）
 *
 * 对应 P0-3 增量 Tool，属于检索类 Tool。
 * 通过 IPC 调用主进程的 fs.readFile。
 *
 * Pre-Gate 规则：
 *   - path 不能为空
 *   - path 必须是绝对路径
 *   - path 必须在资源白名单目录下
 */

import type { ToolResult, PreGateRule } from '../types'
import { createTool } from './toolFactory'

export const readFilePreGateRules: PreGateRule[] = [
  { type: 'not_empty', field: 'path', error: 'path 不能为空' },
  { type: 'regex_match', field: 'path', pattern: '^[/]|^[A-Za-z]:', error: 'path 必须是绝对路径（如 C:\\xxx）' },
  { type: 'regex_not_match', field: 'path', pattern: '(/path/|placeholder|example)', error: '路径似乎是示例路径，请提供真实路径' },
]

/**
 * read_file: 读取指定文本文件的内容（UTF-8 编码），默认最大 100KB。
 *
 * 使用场景：
 *   - 用户需要查看某个配置文件的内容
 *   - 查看业务系统的模型文件（JSON / YAML / TOML）
 *   - 预览文本资源文件
 */
export const readFileTool = createTool({
  name: 'read_file',
  description: '读取指定文本文件的内容（UTF-8 编码，最大 100KB）。用于查看配置文件、模型文件、资源文件等文本内容。',
  category: 'retrieval',
  params: {
    path: {
      type: 'string',
      description: '要读取的文件绝对路径，如 "C:\\Users\\xxx\\public\\models\\room.json"',
      required: true,
    },
    maxSize: {
      type: 'number',
      description: '最大读取字节数（可选，默认 102400，即 100KB）',
      required: false,
    },
  },
  preGates: readFilePreGateRules,

  resultSchema: {
    fields: [
      { name: 'path', type: 'string', label: '文件路径', priority: 1 },
      { name: 'size', type: 'number', label: '文件大小', priority: 1 },
      { name: 'content', type: 'string', label: '文件内容', priority: 2 },
    ],
    successTemplate: '📄 {path} （{size} 字节）',
    emptyTemplate: '📄 {path}: 空文件',
    errorTemplate: '❌ read_file: {error}',
  },

  /** 自定义渲染：📄 文件路径 + 大小 + 内容预览 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const lines: string[] = [`📄 ${d.path}（${d.size} 字节）`]
    const content = String(d.content || '')
    if (content.length > 0) {
      // 取前 15 行作为预览
      const preview = content.split('\n').slice(0, 15).join('\n').slice(0, 600)
      lines.push(preview)
      if (content.length > 600) lines.push('...（内容过长，已截断）')
    }
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const filePath = String(args.path || '')
    const maxSize = Number(args.maxSize) || 102400

    if (!filePath) {
      return { success: false, error: 'path 不能为空' }
    }

    try {
      const result = await window.blessstar.executeTool('read_file', { path: filePath, maxSize })
      if (result.success) {
        const data = typeof result.result === 'string' ? JSON.parse(result.result) : result.result
        return {
          success: true,
          data: {
            path: filePath,
            content: data.content,
            size: data.size || data.content?.length || 0,
          },
        }
      } else {
        return { success: false, error: result.result || '读取文件失败' }
      }
    } catch (err) {
      return { success: false, error: `读取文件时出错: ${(err as Error).message}` }
    }
  },
})
