import type { FunctionTool, ToolResult } from '../types'
import { validateBlessStarSchema, validateGateRule, formatValidationErrors } from '../validator'

export const validateConfigTool: FunctionTool = {
  definition: {
    name: 'validate_config',
    description: '校验配置文件 JSON 是否符合 BlessStar Schema 和 Gate 规则。可校验 Schema 字段或 Gate 链',
    parameters: {
      type: 'object',
      properties: {
        config_json: {
          type: 'string',
          description: '待校验的配置 JSON 字符串（完整 Schema 或 Gate 链 JSON）',
        },
        mode: {
          type: 'string',
          description: '校验模式：schema（Schema 校验）/ gate（Gate 规则校验）/ all（两项都校验）',
          enum: ['schema', 'gate', 'all'],
          default: 'schema',
        },
      },
      required: ['config_json'],
    },
  },

  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    if (d.valid) return [`✅ 配置校验通过 ✓（模式: ${d.mode || 'schema'}）`]
    return ['❌ 配置校验未通过', String(d.error || '未知错误')]
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const configJson = String(args.config_json || '').trim()
    const mode = String(args.mode || 'schema')

    if (!configJson) {
      return { success: false, error: 'config_json 不能为空' }
    }

    const allErrors: string[] = []

    if (mode === 'schema' || mode === 'all') {
      const schemaResult = await validateBlessStarSchema(configJson)
      if (!schemaResult.valid) {
        allErrors.push(`Schema 校验失败：\n${formatValidationErrors(schemaResult)}`)
      }
    }

    if (mode === 'gate' || mode === 'all') {
      const gateResult = await validateGateRule(configJson)
      if (!gateResult.valid) {
        allErrors.push(`Gate 校验失败：\n${formatValidationErrors(gateResult)}`)
      }
    }

    if (allErrors.length > 0) {
      return {
        success: false,
        error: `配置校验未通过：\n${allErrors.join('\n---\n')}`,
      }
    }

    return {
      success: true,
      data: {
        valid: true,
        message: '配置校验通过 ✓',
        mode,
      },
    }
  },
}
