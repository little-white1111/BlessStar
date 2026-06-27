/**
 * read_diagnostics — 读取 BlessStar Schema/Gate 诊断信息
 *
 * 对应 P0-3 增量 Tool，属于执行类 Tool。
 * 读取 Schema 注册中心或 Gate 引擎的当前诊断状态，
 * 用于帮助用户理解配置验证失败的原因。
 */

import type { ToolResult } from '../types'
import { createTool } from './toolFactory'

/**
 * read_diagnostics: 读取当前 BlessStar 系统的诊断信息。
 * 包括 Schema 校验错误、Gate 规则冲突、配置一致性问题等。
 *
 * 使用场景：
 *   - validate_config 返回错误后，用户需要了解具体错误详情
 *   - 用户需要查看当前的诊断日志
 *   - 排查配置问题时查看系统报告
 */
export const readDiagnosticsTool = createTool({
  name: 'read_diagnostics',
  description: '读取 BlessStar 系统的诊断信息，包括 Schema 校验错误、Gate 规则警告等。可用于排查配置问题。',
  category: 'execution',
  params: {
    scope: {
      type: 'string',
      description: '诊断范围（可选），如 "schema" / "gate" / "config" / "all"。默认 "all" 返回所有诊断',
      required: false,
    },
    limit: {
      type: 'number',
      description: '返回的最大诊断条目数（可选，默认 20）',
      required: false,
    },
  },

  resultSchema: {
    fields: [
      { name: 'scope', type: 'string', label: '诊断范围', priority: 1 },
      { name: 'count', type: 'number', label: '诊断数', priority: 1 },
      { name: 'diagnostics', type: 'array', label: '诊断列表', priority: 2 },
    ],
    successTemplate: '🔍 诊断 [{scope}]: {count} 条',
    emptyTemplate: '🔍 诊断 [{scope}]: 未发现问题',
    errorTemplate: '❌ read_diagnostics: {error}',
  },

  /** 自定义渲染：诊断条目列表 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const diagnostics = d.diagnostics as Array<{ level?: string; message?: string; path?: string }> | undefined
    if (!diagnostics || diagnostics.length === 0) return [`🔍 诊断（${d.scope}）：未发现问题`]
    const lines: string[] = [`🔍 诊断（${d.scope}）：${diagnostics.length} 条`]
    for (const diag of diagnostics) {
      const level = diag.level || 'INFO'
      const msg = diag.message || ''
      const path = diag.path ? `[${diag.path}] ` : ''
      lines.push(`  ${level === 'WARN' ? '⚠️' : level === 'ERROR' ? '❌' : 'ℹ️'} [${level}] ${path}${msg}`)
    }
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const scope = String(args.scope || 'all')
    const limit = Number(args.limit) || 20

    try {
      const result = await window.blessstar.executeTool('read_diagnostics', { scope, limit })
      if (result.success) {
        const data = typeof result.result === 'string' ? JSON.parse(result.result) : result.result
        const diagnostics = Array.isArray(data) ? data : (data?.diagnostics || [])
        return {
          success: true,
          data: {
            scope,
            count: diagnostics.length,
            diagnostics,
          },
        }
      } else {
        return { success: false, error: result.result || '读取诊断信息失败' }
      }
    } catch (err) {
      return { success: false, error: `读取诊断时出错: ${(err as Error).message}` }
    }
  },
})
