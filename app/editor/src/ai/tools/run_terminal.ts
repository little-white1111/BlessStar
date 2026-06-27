/**
 * run_terminal — 在受限终端中执行只读命令
 *
 * 对应 P0-3 增量 Tool，属于终端类 Tool（GAP-12 约束：命令白名单 + 目录沙箱）。
 * 仅允许 dir/ls/type/cat/tree/stat 等只读命令，禁止 del/rm/move 等写操作。
 *
 * Pre-Gate 规则：
 *   - command 不能为空
 *   - command 必须在白名单内
 *   - cwd 必须在允许的目录下
 */

import type { ToolResult, PreGateRule } from '../types'
import { createTool } from './toolFactory'

// ── 命令白名单（仅允许只读命令）────────────────────────────────────
const ALLOWED_COMMANDS = ['dir', 'ls', 'type', 'cat', 'tree', 'stat', 'echo', 'findstr', 'grep', 'where']

// ── Pre-Gate 规则 ───────────────────────────────────────────────────

export const runTerminalPreGateRules: PreGateRule[] = [
  { type: 'not_empty', field: 'command', error: 'command 不能为空' },
  { type: 'not_empty', field: 'cwd', error: '工作目录（cwd）不能为空' },
]

/**
 * 自定义校验函数：检查命令是否在白名单内
 */
export function validateCommandAllowed(command: string): string | null {
  const cmdName = command.trim().split(/\s+/)[0].toLowerCase()
  if (!ALLOWED_COMMANDS.includes(cmdName)) {
    return `命令 "${cmdName}" 不在白名单中。允许的命令: ${ALLOWED_COMMANDS.join(', ')}`
  }
  return null
}

/**
 * run_terminal: 在受限终端中执行只读命令。
 * 仅允许读取文件系统信息的命令，禁止任何写入/删除/修改操作。
 *
 * 使用场景：
 *   - 用户需要查看目录结构（tree）
 *   - 查看文件属性信息（stat）
 *   - 使用 findstr/grep 搜索文件内容
 *   - 确认某个路径是否存在（dir/ls）
 */
export const runTerminalTool = createTool({
  name: 'run_terminal',
  description: `在受限终端中执行只读命令。仅允许: ${ALLOWED_COMMANDS.join(', ')}。禁止写入/删除/修改操作。工作目录必须在项目资源目录下。`,
  category: 'terminal',
  approvalRequired: true,
  params: {
    command: {
      type: 'string',
      description: '要执行的命令，如 "dir /b"、"tree /f"、"type config.json" 等',
      required: true,
    },
    cwd: {
      type: 'string',
      description: '命令的工作目录绝对路径',
      required: true,
    },
  },
  preGates: runTerminalPreGateRules,

  resultSchema: {
    fields: [
      { name: 'command', type: 'string', label: '执行的命令', priority: 1 },
      { name: 'cwd', type: 'string', label: '工作目录', priority: 1 },
      { name: 'output', type: 'string', label: '命令输出', priority: 2 },
      { name: 'exitCode', type: 'number', label: '退出码', priority: 2 },
    ],
    successTemplate: '💻 [{cwd}]$ {command} （退出码: {exitCode}）',
    errorTemplate: '❌ run_terminal: {error}',
  },

  /** 自定义渲染：💻 命令 + 输出（截断过长输出） */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 命令执行失败']
    const command = String(d.command || '')
    const cwd = String(d.cwd || '')
    const output = String(d.output || '')
    const exitCode = d.exitCode ?? 0
    const lines: string[] = [`💻 [${cwd}]$ ${command}（退出码: ${exitCode}）`]
    if (output.length > 0) {
      const outLines = output.split('\n')
      const maxLines = 20
      const truncated = outLines.slice(0, maxLines)
      for (const line of truncated) {
        lines.push(`  ${line}`)
      }
      if (outLines.length > maxLines) {
        lines.push(`  ...（共 ${outLines.length} 行，仅显示前 ${maxLines} 行）`)
      }
    }
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const command = String(args.command || '')
    const cwd = String(args.cwd || '')

    if (!command || !cwd) {
      return { success: false, error: 'command 和 cwd 不能为空' }
    }

    // 命令白名单检查
    const cmdError = validateCommandAllowed(command)
    if (cmdError) {
      return { success: false, error: `[安全限制] ${cmdError}` }
    }

    try {
      const result = await window.blessstar.executeTool('run_terminal', { command, cwd })
      if (result.success) {
        // 安全解析：先尝试 JSON，失败了用原始字符串
        let data: any
        try {
          data = typeof result.result === 'string' ? JSON.parse(result.result) : result.result
        } catch {
          data = { output: String(result.result || ''), exitCode: 0 }
        }
        return {
          success: true,
          data: {
            command,
            cwd,
            output: data.output || String(data),
            exitCode: data.exitCode ?? 0,
          },
        }
      } else {
        // 尝试从 JSON 结果中提取错误信息
        let errorMsg = String(result.result || '命令执行失败')
        try {
          const parsed = JSON.parse(errorMsg)
          errorMsg = parsed.output || parsed.error || errorMsg
        } catch { /* not JSON, use raw string */ }
        return { success: false, error: errorMsg }
      }
    } catch (err) {
      return { success: false, error: `命令执行时出错: ${(err as Error).message}` }
    }
  },
})
