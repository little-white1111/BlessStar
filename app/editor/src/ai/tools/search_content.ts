/**
 * search_content — 在指定目录下的文本文件中搜索关键词
 *
 * 对应 P0-3 增量 Tool，属于检索类 Tool。
 * 通过 IPC 调用主进程的 grep/findstr 等效操作。
 *
 * Pre-Gate 规则：
 *   - path 不能为空
 *   - path 必须是绝对路径
 *   - pattern 不能为空
 */

import type { ToolResult, PreGateRule } from '../types'
import { createTool } from './toolFactory'

export const searchContentPreGateRules: PreGateRule[] = [
  { type: 'not_empty', field: 'path', error: 'path 不能为空' },
  { type: 'not_empty', field: 'pattern', error: '搜索关键词不能为空' },
  { type: 'regex_match', field: 'path', pattern: '^[/]|^[A-Za-z]:', error: 'path 必须是绝对路径（如 C:\\xxx）' },
  { type: 'regex_not_match', field: 'path', pattern: '(/path/|placeholder|example)', error: '路径似乎是示例路径，请提供真实路径' },
]

/**
 * search_content: 在指定目录下搜索包含关键词的文本文件。
 * 非递归搜索第一层，支持 *.json / *.yaml / *.toml / *.txt 等常见文本格式。
 *
 * 使用场景：
 *   - 用户需要查找"所有包含'room_id'的配置文件"
 *   - 搜索业务模型中的某个字段引用
 *   - 查找某个值在哪些文件中出现
 */
export const searchContentTool = createTool({
  name: 'search_content',
  description: '在指定目录中搜索包含指定关键词的文件。返回匹配的文件名列表（非递归，仅第一层）。支持文本文件搜索。',
  category: 'retrieval',
  params: {
    path: {
      type: 'string',
      description: '要搜索的目录绝对路径，如 "C:\\Users\\xxx\\public\\models"',
      required: true,
    },
    pattern: {
      type: 'string',
      description: '要搜索的关键词或正则表达式',
      required: true,
    },
    filePattern: {
      type: 'string',
      description: '文件过滤模式（可选），如 "*.json"、"*.yaml"，默认搜索所有文本文件',
      required: false,
    },
  },
  preGates: searchContentPreGateRules,

  resultSchema: {
    fields: [
      { name: 'path', type: 'string', label: '搜索目录', priority: 1 },
      { name: 'pattern', type: 'string', label: '搜索关键词', priority: 1 },
      { name: 'count', type: 'number', label: '匹配数', priority: 1 },
      { name: 'matches', type: 'array', label: '匹配文件', priority: 2 },
    ],
    successTemplate: '🔍 在 {path} 中找到 {count} 个包含 "{pattern}" 的文件',
    emptyTemplate: '🔍 在 {path} 中未找到包含 "{pattern}" 的文件',
    errorTemplate: '❌ search_content: {error}',
  },

  /** 自定义渲染：搜索命中列表 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const matches = d.matches as string[] | undefined
    if (!matches || matches.length === 0) return [`🔍 在 ${d.path} 中未找到包含 "${d.pattern}" 的文件`]
    const lines: string[] = [`🔍 在 ${d.path} 中搜索 "${d.pattern}"：${matches.length} 个匹配文件`]
    for (const m of matches) {
      lines.push(`  📄 ${m}`)
    }
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const dirPath = String(args.path || '')
    const pattern = String(args.pattern || '')
    const filePattern = args.filePattern ? String(args.filePattern) : ''

    if (!dirPath || !pattern) {
      return { success: false, error: 'path 和 pattern 不能为空' }
    }

    try {
      const result = await window.blessstar.executeTool('search_content', {
        path: dirPath,
        pattern,
        filePattern: filePattern || undefined,
      })
      if (result.success) {
        const data = typeof result.result === 'string' ? JSON.parse(result.result) : result.result
        const matches = Array.isArray(data) ? data : []
        return {
          success: true,
          data: {
            path: dirPath,
            pattern,
            count: matches.length,
            matches,
          },
        }
      } else {
        return { success: false, error: result.result || '搜索失败' }
      }
    } catch (err) {
      return { success: false, error: `搜索时出错: ${(err as Error).message}` }
    }
  },
})
