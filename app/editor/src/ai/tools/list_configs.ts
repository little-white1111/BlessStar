/**
 * list_configs — 批量读取所有已声明的配置字段
 *
 * 当用户询问"有哪些配置""当前配置是什么"时，一次性返回所有字段的 key/value/type。
 * 避免 AI 逐个调用 read_config_value（46 次 function call 不现实）。
 */

import type { ToolResult } from '../types'
import { createTool } from './toolFactory'
import { keyToLabel } from './configLabels'

/** 格式化单个配置为人类可读行 */
function formatConfig(config: { key: string; value: string | null; type?: string; default?: string }): string {
  const label = keyToLabel(config.key)
  const value = config.value ?? config.default ?? ''
  // 空值显示为 "未设置"
  const displayValue = (value === '' || value === null || value === undefined) ? '未设置' : value
  return `${label}（${config.key}）：${displayValue}`
}

export const listConfigsTool = createTool({
  name: 'list_configs',
  description: '批量读取所有已声明的配置字段及其当前值。用于回答"当前有哪些配置""查看所有配置"等查询。无需参数。',
  category: 'retrieval',
  approvalRequired: false,
  params: {
    prefix: {
      type: 'string',
      description: '可选：只列出 key 以此前缀开头的配置项（如 "livedesign.room"）。不传则列出全部。',
      required: false,
    },
  },

  resultSchema: {
    fields: [
      { name: 'count', type: 'number', label: '字段总数', priority: 1 },
      { name: 'configs', type: 'array', label: '配置列表', priority: 1 },
    ],
    successTemplate: '共 {count} 个配置字段',
    emptyTemplate: '暂无已声明的配置字段',
    errorTemplate: '❌ list_configs: {error}',
  },

  /** 自定义渲染：人类可读的 描述（key）：值 格式 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const configs = d.configs as Array<{ key: string; value: string | null; type?: string; default?: string }> | undefined
    if (!configs || configs.length === 0) return ['暂无配置字段']
    // 按 key 排序
    const sorted = [...configs].sort((a, b) => a.key.localeCompare(b.key))
    return sorted.map(c => formatConfig(c))
  },

  async execute(_args: Record<string, unknown>): Promise<ToolResult> {
    try {
      const prefix = _args.prefix ? String(_args.prefix) : ''
      const result = await window.blessstar.executeTool('list_configs', prefix ? { prefix } : {})
      if (result.success) {
        const data = typeof result.result === 'string' ? JSON.parse(result.result) : result.result
        return { success: true, data }
      } else {
        return { success: false, error: String(result.result || '读取配置失败') }
      }
    } catch (err) {
      return { success: false, error: `读取配置时出错: ${(err as Error).message}` }
    }
  },
})
