import type { ToolResult, PreGateRule } from '../types'
import { createTool } from './toolFactory'
import { keyToLabel } from './configLabels'

/**
 * write_config_value Pre-Gate 规则：入参前置校验
 * 拒绝负数值（如房间号 -1）以保护 Schema 完整性
 */
export const writeConfigValuePreGateRules: PreGateRule[] = [
  { type: 'regex_not_match', field: 'value', pattern: '^-\\d+', error: '值不能为负数' },
  { type: 'not_empty', field: 'key', error: 'key 不能为空' },
]

/**
 * write_config_value: 将单个配置值写入 BlessStar 运行时值存储。
 * 对已通过 registerConfigFields() 声明过的字段有效。
 * 写入后可通过 read_config_value 读取验证。
 *
 * 使用场景：
 *   - 修改配置项的运行时值
 *   - 对任意已注册字段的运行时值进行修改
 */
export const writeConfigValueTool = createTool({
  name: 'write_config_value',
  description: '将单个配置值写入 BlessStar 运行时存储（直接生效，不需确认）。请与 read_config_value 配合使用先读后写。',
  category: 'execution',
  approvalRequired: false,
  params: {
    key: {
      type: 'string',
      description: '字段完整 key（如 "some.domain.field_name"）',
      required: true,
    },
    value: {
      type: 'string',
      description: '要设置的字符串值（数字、布尔值也用字符串表达，如 "100"、"true"、"14"）',
      required: true,
    },
  },
  preGates: writeConfigValuePreGateRules,

  resultSchema: {
    fields: [
      { name: 'key', type: 'string', label: '字段', priority: 1 },
      { name: 'value', type: 'string', label: '新值', priority: 1 },
      { name: 'warning', type: 'string', label: '警告', priority: 2 },
    ],
    successTemplate: '✅ {key} 已写入 "{value}"',
    errorTemplate: '❌ write_config_value: {error}',
  },

  /** 自定义渲染：中文描述（key）已设为 value */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['写入失败']
    const dataObj = d.written as Record<string, string> | undefined
    const key = dataObj?.key || String(d.key || '')
    const value = dataObj?.value || String(d.value || '')
    const warning = String(d.warning || '')
    const label = keyToLabel(key)
    const lines = [`${label}（${key}）已设为 ${value}`]
    if (warning) lines.push(`⚠ ${warning}`)
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const key = String(args.key || '')
    const value = String(args.value || '')

    if (!key) {
      return { success: false, error: 'key 不能为空' }
    }

    try {
      const raw: { success: boolean; result?: string; warning?: string } =
        await window.blessstar.executeTool('write_config_value', { key, value }) as any
      if (raw.success) {
        let message = `✅ ${key} 已成功设置为 "${value}"`
        if (raw.warning) {
          message += `（⚠ ${raw.warning}）`
        }
        return {
          success: true,
          data: {
            written: { key, value },
            value,
            key,
            warning: raw.warning,
            message,
          },
        }
      } else {
        return { success: false, error: raw.result || '写入失败' }
      }
    } catch (err) {
      return { success: false, error: `写入配置时出错: ${(err as Error).message}` }
    }
  },
})
