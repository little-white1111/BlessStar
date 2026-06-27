import type { FunctionTool, ToolResult } from '../types'
import { validateBlessStarSchema, formatValidationErrors } from '../validator'

export const createSchemaFieldTool: FunctionTool = {
  definition: {
    name: 'create_schema_field',
    description: '在 Schema 配置中创建一个新的字段定义',
    parameters: {
      type: 'object',
      properties: {
        key: {
          type: 'string',
          description: '字段标识符（字母数字下划线，如 "server_port"）',
        },
        widget: {
          type: 'string',
          description: '控件类型：input / select / checkbox / radio / number / textarea / group / repeatable',
          enum: ['input', 'select', 'checkbox', 'radio', 'number', 'textarea', 'group', 'repeatable'],
        },
        label: {
          type: 'string',
          description: '界面显示标签',
        },
        required: {
          type: 'boolean',
          description: '是否为必填字段',
          default: false,
        },
        placeholder: {
          type: 'string',
          description: '输入提示文本',
        },
        description: {
          type: 'string',
          description: '字段描述说明',
        },
        default_value: {
          type: 'string',
          description: '默认值',
        },
      },
      required: ['key', 'widget', 'label'],
    },
  },

  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const field = d.field as Record<string, unknown> | undefined
    if (!field) return [`✅ Schema JSON:\n${String(d.fieldJson || '')}`]
    const lines: string[] = ['✅ 已创建 Schema 字段']
    if (field.key) lines.push(`  字段名: ${field.key}`)
    if (field.widget) lines.push(`  控件类型: ${field.widget}`)
    if (field.label) lines.push(`  标签: ${field.label}`)
    if (field.required) lines.push('  状态: 必填')
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const key = String(args.key || '')
    const widget = String(args.widget || '')
    const label = String(args.label || '')

    if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/)) {
      return { success: false, error: `字段标识符 "${key}" 格式无效，须以字母或下划线开头，仅含字母数字下划线` }
    }

    const fieldJson = JSON.stringify({
      key, widget, label,
      required: args.required ?? false,
      placeholder: args.placeholder || undefined,
      description: args.description || undefined,
      default_value: args.default_value || undefined,
    }, null, 2)

    // Validate via BlessStar Schema validator
    const validation = await validateBlessStarSchema(JSON.stringify({
      version: '1.0',
      title: 'Schema 字段',
      fields: [{ key, widget, label }],
    }))

    if (!validation.valid) {
      return {
        success: false,
        error: `字段定义未能通过 Schema 校验：\n${formatValidationErrors(validation)}`,
      }
    }

    return {
      success: true,
      data: {
        fieldJson,
        field: { key, widget, label, required: args.required ?? false },
      },
    }
  },
}
