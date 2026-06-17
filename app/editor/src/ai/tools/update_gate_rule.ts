import type { FunctionTool, ToolResult } from '../types'
import { validateGateRule, formatValidationErrors } from '../validator'

export const updateGateRuleTool: FunctionTool = {
  definition: {
    name: 'update_gate_rule',
    description: '更新或添加 Gate 门禁规则，用于配置校验逻辑（gate_id + scenario + 规则组合）',
    parameters: {
      type: 'object',
      properties: {
        gate_id: {
          type: 'string',
          description: 'Gate 标识符，如 "main_gate"、"security_gate"',
        },
        scenario: {
          type: 'string',
          description: '场景名称，如 "production"、"development"',
        },
        action: {
          type: 'string',
          description: '操作类型：add_rule / update_rule / remove_rule',
          enum: ['add_rule', 'update_rule', 'remove_rule'],
        },
        field: {
          type: 'string',
          description: '目标字段名',
        },
        operator: {
          type: 'string',
          description: '比较运算符：eq / neq / gt / lt / gte / lte / in / match',
          enum: ['eq', 'neq', 'gt', 'lt', 'gte', 'lte', 'in', 'match'],
        },
        value: {
          type: 'string',
          description: '比较值',
        },
      },
      required: ['gate_id', 'scenario', 'action'],
    },
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const gate_id = String(args.gate_id || '')
    const scenario = String(args.scenario || '')
    const action = String(args.action || '')
    const field = args.field ? String(args.field) : undefined
    const operator = args.operator ? String(args.operator) : undefined
    const value = args.value ? String(args.value) : undefined

    const gateJson = JSON.stringify({
      gate_id, scenario,
      action,
      field,
      condition: operator ? { operator, value } : undefined,
    }, null, 2)

    // Validate via Gate rule validator
    const validation = await validateGateRule(JSON.stringify({
      gate_id, scenario,
      do: [{ type: 'meta_rule', field: field || 'target', operator: operator || 'eq', value: value || '' }],
    }))

    if (!validation.valid) {
      return {
        success: false,
        error: `Gate 规则未能通过校验：\n${formatValidationErrors(validation)}`,
      }
    }

    return {
      success: true,
      data: { gateJson, gate_id, scenario, action, field },
    }
  },
}
