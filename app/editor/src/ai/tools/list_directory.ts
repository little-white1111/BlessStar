import type { ToolResult, PreGateRule } from '../types'
import { createTool } from './toolFactory'

/**
 * list_directory Pre-Gate 规则：入参前置校验
 * 对应缺口四（只校验输出，不校验输入）的 BlessStar-native 方案。
 */
export const listDirectoryPreGateRules: PreGateRule[] = [
  { type: 'not_empty', field: 'path', error: 'path 不能为空' },
  { type: 'regex_match', field: 'path', pattern: '^[/]|^[A-Za-z]:', error: 'path 必须是绝对路径（如 C:\\xxx）' },
  { type: 'regex_not_match', field: 'path', pattern: '(/path/|placeholder|example)', error: '路径似乎是示例路径，请提供真实路径' },
]

/**
 * list_directory: 列出指定目录下的文件和子目录（非递归，仅第一层）。
 * 通过 IPC 调用主进程的 fs.readdir + fs.stat。
 *
 * 使用场景：
 *   - 用户问"当前配置目录有什么文件" → 读取配置目录
 *   - 用户需要知道某个路径下有哪些文件
 */
export const listDirectoryTool = createTool({
  name: 'list_directory',
  description: '列出指定目录下的文件和子目录（仅第一层，区分文件和目录）',
  params: {
    path: {
      type: 'string',
      description: '要读取的目录绝对路径，如 "C:\\Users\\xxx\\public\\models"',
      required: true,
    },
  },
  preGates: listDirectoryPreGateRules,

  // resultSchema：Tool Declaration 声明中同时覆盖 delta formatter + renderer
  resultSchema: {
    fields: [
      { name: 'path', type: 'string', label: '目录路径', priority: 1 },
      { name: 'count', type: 'number', label: '项目数', priority: 1 },
      { name: 'entries', type: 'array', label: '内容', priority: 2 },
    ],
    successTemplate: '📂 {path} ({count} 项)',
    emptyTemplate: '📂 {path}: 空目录',
    errorTemplate: '❌ list_directory: {error}',
  },

  /** 自定义渲染：目录条目列表 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const entries = d.entries as Array<{ name?: string; isDirectory?: boolean }> | undefined
    if (!entries || entries.length === 0) return [`📂 ${d.path || ''}: 空目录`]
    const lines: string[] = [`📂 ${d.path}（${entries.length} 项）`]
    for (const entry of entries) {
      if (entry.isDirectory) {
        lines.push(`  📁 ${entry.name}/`)
      } else {
        lines.push(`  📄 ${entry.name}`)
      }
    }
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const dirPath = String(args.path || '')

    if (!dirPath) {
      return { success: false, error: 'path 不能为空' }
    }

    try {
      const result = await window.blessstar.executeTool('list_directory', { path: dirPath })
      if (result.success) {
        const data = typeof result.result === 'string' ? JSON.parse(result.result) : result.result
        return {
          success: true,
          data: {
            path: dirPath,
            entries: data,
            count: Array.isArray(data) ? data.length : 0,
          },
        }
      } else {
        return { success: false, error: result.result || '读取目录失败' }
      }
    } catch (err) {
      return { success: false, error: `读取目录时出错: ${(err as Error).message}` }
    }
  },
})
