/**
 * find_files — 通过 glob 模式查找文件
 *
 * 对应 P0-3 增量 Tool，属于检索类 Tool。
 * 通过 IPC 调用主进程的 glob/fs 操作。
 *
 * Pre-Gate 规则：
 *   - path 不能为空
 *   - path 必须是绝对路径
 *   - pattern 不能为空
 */

import type { ToolResult, PreGateRule } from '../types'
import { createTool } from './toolFactory'

export const findFilesPreGateRules: PreGateRule[] = [
  { type: 'not_empty', field: 'path', error: 'path 不能为空' },
  { type: 'not_empty', field: 'pattern', error: 'glob 模式不能为空' },
  { type: 'regex_match', field: 'path', pattern: '^[/]|^[A-Za-z]:', error: 'path 必须是绝对路径（如 C:\\xxx）' },
  { type: 'regex_not_match', field: 'path', pattern: '(/path/|placeholder|example)', error: '路径似乎是示例路径，请提供真实路径' },
]

/**
 * find_files: 根据 glob 模式在指定目录中查找文件（非递归）。
 *
 * 使用场景：
 *   - 用户需要查找"所有 .json 文件"
 *   - 查找某个目录下的所有模型文件
 *   - 查找特定命名的文件
 */
export const findFilesTool = createTool({
  name: 'find_files',
  description: '在指定目录中根据文件匹配模式（glob）查找文件。支持 *.json、*.yaml、config_*.* 等模式。仅第一层非递归。',
  category: 'retrieval',
  params: {
    path: {
      type: 'string',
      description: '要查找的目录绝对路径，如 "C:\\Users\\xxx\\public\\models"',
      required: true,
    },
    pattern: {
      type: 'string',
      description: '文件匹配模式（glob），如 "*.json"、"config_*.*"、"room_*.yaml"',
      required: true,
    },
  },
  preGates: findFilesPreGateRules,

  resultSchema: {
    fields: [
      { name: 'path', type: 'string', label: '查找目录', priority: 1 },
      { name: 'pattern', type: 'string', label: '匹配模式', priority: 1 },
      { name: 'count', type: 'number', label: '文件数', priority: 1 },
      { name: 'files', type: 'array', label: '文件列表', priority: 2 },
    ],
    successTemplate: '📁 在 {path} 中找到 {count} 个匹配 "{pattern}" 的文件',
    emptyTemplate: '📁 在 {path} 中未找到匹配 "{pattern}" 的文件',
    errorTemplate: '❌ find_files: {error}',
  },

  /** 自定义渲染：文件匹配列表 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const files = d.files as string[] | undefined
    if (!files || files.length === 0) return [`📁 在 ${d.path} 中未找到匹配 "${d.pattern}" 的文件`]
    const lines: string[] = [`📁 在 ${d.path} 中匹配 "${d.pattern}"：${files.length} 个文件`]
    for (const f of files) {
      lines.push(`  📄 ${f}`)
    }
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const dirPath = String(args.path || '')
    const pattern = String(args.pattern || '')

    if (!dirPath || !pattern) {
      return { success: false, error: 'path 和 pattern 不能为空' }
    }

    try {
      const result = await window.blessstar.executeTool('find_files', { path: dirPath, pattern })
      if (result.success) {
        const data = typeof result.result === 'string' ? JSON.parse(result.result) : result.result
        const files = Array.isArray(data) ? data : []
        return {
          success: true,
          data: {
            path: dirPath,
            pattern,
            count: files.length,
            files,
          },
        }
      } else {
        return { success: false, error: result.result || '查找文件失败' }
      }
    } catch (err) {
      return { success: false, error: `查找文件时出错: ${(err as Error).message}` }
    }
  },
})
