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

  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const lines: string[] = ['✅ 已更新 Gate 规则']
    if (d.gate_id) lines.push(`  Gate: ${d.gate_id}`)
    if (d.scenario) lines.push(`  场景: ${d.scenario}`)
    if (d.action) lines.push(`  操作: ${d.action}`)
    if (d.field) lines.push(`  字段: ${d.field}`)
    return lines
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

    // Validate via Gate rule validator（remove_rule 不需要校验 rule 内容）
    if (action !== 'remove_rule') {
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
    }

    // ── 第34天 · GR-01：调用 registerGate IPC 注册/删除规则 ──
    try {
      if (action === 'remove_rule') {
        // 删除规则：只需 gate_id
        const ruleJson = JSON.stringify({
          type: 'policy',
          gate_id,
          action: 'remove_rule',
        })
        const result = await window.blessstar.registerGate('policy', ruleJson)
        if (!result.success) {
          return { success: false, error: `Gate 删除失败: ${result.error || '未知错误'}` }
        }
      } else if (field && operator) {
        // add/update：构造完整 policy gate
        const ruleJson = JSON.stringify({
          type: 'policy',
          gate_id,
          scenario: scenario || 'production',
          action: action || 'add_rule',
          metadata_rules: [
            {
              instr_name: gate_id,
              key: field,
              op: operator,
              value: value || '',
            },
          ],
        })
        const result = await window.blessstar.registerGate('policy', ruleJson)
        if (!result.success) {
          return { success: false, error: `Gate 注册失败: ${result.error || '未知错误'}` }
        }
      }
    } catch (e) {
      return { success: false, error: `Gate 注册异常: ${(e as Error).message}` }
    }

    return {
      success: true,
      data: { gateJson, gate_id, scenario, action, field },
    }
  },
}
