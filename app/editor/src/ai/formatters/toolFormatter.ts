/**
 * toolFormatter — 工具标签与结果格式化
 *
 * 提供 5 个工具展示相关的纯函数，供 AIPanel.tsx 和 SandboxTodo 使用。
 * 原位置：AIPanel.tsx（臃肿拆分）。
 */

import type { ToolCall, ToolResult } from '../types'
import { getConfigLabelMap } from '../context-manager/adaptiveIndex'

// ── 工具标签 ──────────────────────────────────────────────────────────

/** 工具名称 → 中文标签映射 */
export const TOOL_LABELS: Record<string, string> = {
  write_config_value: '设置配置',
  read_config_value: '读取配置',
  list_configs: '列出配置',
  create_schema_field: '创建 Schema 字段',
  create_gate_chain: '创建 Gate 规则链',
  update_gate_rule: '更新 Gate 规则',
  validate_config: '校验配置',
  generate_normalizer_template: '生成归一化模板',
  list_directory: '列出目录',
  read_file: '读取文件',
  search_content: '搜索内容',
  find_files: '查找文件',
  run_terminal: '终端命令',
  read_diagnostics: '诊断信息',
  chat: '对话',
}

/** 返回工具的中文标签 */
export function toolLabel(name: string): string {
  return TOOL_LABELS[name] || name
}

// ── 目录/配置格式化 ──────────────────────────────────────────────────

/** 格式化目录列表为中文行 */
export function formatDirLines(data: unknown): string[] {
  if (Array.isArray(data)) {
    if (data.length === 0) return ['(目录为空)']
    return data.map((item: unknown) => {
      if (typeof item === 'string') return item
      const obj = item as Record<string, unknown>
      return obj.name ? `${obj.type === 'dir' ? '📁' : '📄'} ${obj.name}` : JSON.stringify(item)
    })
  }
  return [typeof data === 'string' ? data : JSON.stringify(data, null, 2)]
}

/** 通用的工具结果渲染行 */
export function resultLines(toolName: string, result: ToolResult): string[] {
  if (!result.success || !result.data) {
    return [result.error || '操作失败']
  }
  if (toolName === 'list_directory') {
    return formatDirLines(result.data)
  }
  if (toolName === 'list_configs') {
    const obj = result.data as Record<string, unknown>
    const configs = obj.configs as Array<Record<string, unknown>> | undefined
    if (!configs || !Array.isArray(configs)) {
      return [JSON.stringify(result.data, null, 2)]
    }
    const labelMap = getConfigLabelMap()
    return configs.map((c) => {
      const key = String(c.key || '')
      const label = labelMap[key] || key.split('.').pop() || key
      const val = c.value != null ? String(c.value) : null
      return val
        ? `${label}：${key} = ${val}`
        : `${label}：${key} 未设置`
    })
  }
  if (Array.isArray(result.data)) {
    return result.data.map((item: unknown) =>
      typeof item === 'string' ? item : JSON.stringify(item))
  }
  if (typeof result.data === 'string') {
    return result.data.split('\n').filter(Boolean)
  }
  return [JSON.stringify(result.data, null, 2)]
}

/** 格式化 tool results 为文本块 */
export function toolResultsText(
  toolCalls: ToolCall[],
  results: ToolResult[],
): string {
  return toolCalls.map((tc, i) => {
    const r = results[i]
    const label = toolLabel(tc.function.name)
    return r.success
      ? `✅ ${label}: ${typeof r.data === 'string' ? r.data : JSON.stringify(r.data)}`
      : `❌ ${label}: ${r.error || '失败'}`
  }).join('\n')
}
