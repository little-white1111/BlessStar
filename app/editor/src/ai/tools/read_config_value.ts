import type { ToolResult, PreGateRule } from '../types'
import { createTool } from './toolFactory'
import { keyToLabel } from './configLabels'

/**
 * read_config_value Pre-Gate 规则：key 不能为空
 */
export const readConfigValuePreGateRules: PreGateRule[] = [
  { type: 'not_empty', field: 'key', error: 'key 不能为空' },
]

/**
 * read_config_value: 从 BlessStar 运行时值存储读取单个配置值。
 * 优先读取运行时值（write_config_value 写入的），未找到则读取声明默认值。
 *
 * 使用场景：
 *   - 验证 write_config_value 是否写入成功
 *   - 查看当前某个字段的值
 *   - 用户询问"当前某个配置项的值是多少"
 */
export const readConfigValueTool = createTool({
  name: 'read_config_value',
  description: '从 BlessStar 运行时存储读取单个配置值。如果 key 不存在返回 null，存在则返回字符串值。请与 write_config_value 配合使用。',
  category: 'retrieval',
  approvalRequired: false,
  params: {
    key: {
      type: 'string',
      description: '字段完整 key（如 "some.domain.field_name"）',
      required: true,
    },
  },
  preGates: readConfigValuePreGateRules,

  resultSchema: {
    fields: [
      { name: 'key', type: 'string', label: '字段', priority: 1 },
      { name: 'value', type: 'string', label: '当前值', priority: 1 },
    ],
    successTemplate: '{key} = {value}',
    emptyTemplate: '⚠️ {key} 未设置值',
    errorTemplate: '❌ read_config_value: {error}',
  },

  /** 自定义渲染：中文描述（key）：当前值 */
  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const key = String(d.key || '')
    const val = d.value
    const label = keyToLabel(key)
    const displayValue = (val === null || val === undefined || val === '') ? '未设置' : String(val)
    return [`${label}（${key}）：${displayValue}`]
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const key = String(args.key || '')

    if (!key) {
      return { success: false, error: 'key 不能为空' }
    }

    try {
      const result = await window.blessstar.executeTool('read_config_value', { key })
      if (result.success) {
        const val = result.result === null ? null : String(result.result)
        const isEmpty = val === null || val === ''

        // 尝试读取该字段的 enumOptions（D38-9-INV-05）
        let enumOptions: string[] | undefined
        try {
          const schema = await (window as any).blessstar?.getRegisteredSchemas?.()
          if (schema?.fields) {
            const field = (schema.fields as Array<{ key: string; enum?: string[] }>).find((f: any) => f.key === key)
            if (field?.enum && field.enum.length > 0) {
              enumOptions = field.enum
            }
          }
        } catch { /* enumOptions 读取失败不阻断 */ }

        return {
          success: true,
          data: {
            key,
            value: val,
            found: val !== null,
            enumOptions,
            message: isEmpty
              ? `⚠️ 配置字段 "${key}" 未设置值，当前为空`
              : `${key} = "${val}"`,
          },
        }
      } else {
        return { success: false, error: result.result || '读取失败' }
      }
    } catch (err) {
      return { success: false, error: `读取配置时出错: ${(err as Error).message}` }
    }
  },
})
