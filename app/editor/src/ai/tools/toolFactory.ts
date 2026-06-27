/**
 * toolFactory — Tool Declaration → 全套组件自动生成
 *
 * 对应缺口七（注册约束）的 BlessStar-native 方案。
 * 一份 ToolDeclaration 声明，工厂自动生成：
 * - definition（FunctionToolParam，供 OpenAI API 用）
 * - deltaFormatter（从 resultSchema 自动推导）
 * - inputValidator（从 preGates 自动推导）
 * - resultRenderer（从 resultSchema 自动推导）
 *
 * 与缺口二的 Tool Result Schema 合并——declaration 中已含 resultSchema。
 */

import type {
  ToolDeclaration,
  FunctionTool,
  ToolResultSchema,
  ToolDelta,
  PreGateRule,
  ToolCategory,
} from '../types'

// ── 从 declaration 构建 FunctionToolParam ────────────────────────────

function buildDefinition(decl: ToolDeclaration): FunctionTool['definition'] {
  const properties: Record<string, unknown> = {}
  const required: string[] = []

  for (const [name, param] of Object.entries(decl.params)) {
    properties[name] = { type: param.type, description: param.description }
    if (param.required) {
      required.push(name)
    }
  }

  return {
    name: decl.name,
    description: decl.description,
    parameters: {
      type: 'object',
      properties,
      required: required.length > 0 ? required : undefined,
    },
  }
}

// ── 从 resultSchema 自动推导 deltaFormatter ─────────────────────────

function buildDeltaFormatter(name: string, schema: ToolResultSchema) {
  return (result: Record<string, unknown>): ToolDelta => {
    if (!result.success) {
      const error = result.error as string
      const tpl = schema.errorTemplate || '❌ {toolName}: {error}'
      return { summary: tpl.replace('{toolName}', name).replace('{error}', error || '未知错误') }
    }

    const data = (result.data || result) as Record<string, unknown>

    // 检测是否空结果
    if (schema.emptyTemplate && (!data || (Array.isArray(data.entries) && data.entries.length === 0))) {
      return { summary: fillTemplate(schema.emptyTemplate, data, schema) }
    }

    return { summary: fillTemplate(schema.successTemplate, data, schema) }
  }
}

function fillTemplate(tpl: string, data: Record<string, unknown>, schema: ToolResultSchema): string {
  let result = tpl
  for (const field of schema.fields) {
    const val = data[field.name]
    if (val !== undefined && val !== null) {
      const strVal = Array.isArray(val) ? val.join(', ') : String(val)
      result = result.replace(`{${field.name}}`, strVal)
    } else {
      result = result.replace(`{${field.name}}`, '')
    }
  }
  return result
}

// ── 从 preGates 自动推导 inputValidator ─────────────────────────────

function buildInputValidator(preGates: PreGateRule[] | undefined) {
  return (args: Record<string, unknown>): string | null => {
    if (!preGates || preGates.length === 0) return null

    for (const rule of preGates) {
      const value = args[rule.field]
      if (rule.type === 'not_empty') {
        if (value === undefined || value === null || value === '') {
          return rule.error
        }
      } else if (rule.type === 'regex_match' && rule.pattern) {
        if (typeof value !== 'string' || !new RegExp(rule.pattern).test(value)) {
          return rule.error
        }
      } else if (rule.type === 'regex_not_match' && rule.pattern) {
        if (typeof value === 'string' && new RegExp(rule.pattern).test(value)) {
          return rule.error
        }
      }
    }
    return null
  }
}

// ── 从 resultSchema 自动推导 resultRenderer ────────────────────────

function buildResultRenderer(name: string, schema: ToolResultSchema) {
  return (data: unknown): string[] => {
    const d = data as Record<string, unknown> | undefined
    if (!d) return [`✅ ${name} 成功`]

    const lines: string[] = []
    for (const field of schema.fields) {
      const val = d[field.name]
      if (val !== undefined && val !== null) {
        let display: string
        if (Array.isArray(val)) {
          // 数组元素若为对象则 JSON.stringify，避免 [object Object]
          const items = (val as unknown[]).map(v => {
            if (v !== null && typeof v === 'object' && !Array.isArray(v)) {
              return JSON.stringify(v)
            }
            return String(v)
          })
          // 超过 8 项时截断并注明总数
          if (items.length > 8) {
            display = `[${items.slice(0, 5).join(', ')} ... ${items.length - 5} more]`
          } else {
            display = `[${items.join(', ')}]`
          }
        } else {
          display = String(val)
        }
        // 只显示优先级 1~2 的字段（高优先级）
        if (field.priority <= 2) {
          lines.push(`${field.label}: ${display}`)
        }
      }
    }
    return lines.length > 0 ? lines : [`✅ ${name} 成功`]
  }
}

// ── 核心工厂函数 ────────────────────────────────────────────────────────

/**
 * 从 ToolDeclaration 生成全套组件：
 * - definition（供 OpenAI API / clean format 使用）
 * - deltaFormatter（从 resultSchema 自动推导，对应缺口二）
 * - inputValidator（从 preGates 自动推导，对应缺口四）
 * - resultRenderer（从 resultSchema 自动推导，对应缺口七）
 * - execute（透传 declaration 中的执行函数）
 *
 * 新增工具的唯一犯错方式：declaration 写错——编译期检查声明完整性。
 */
export function createTool(decl: ToolDeclaration): FunctionTool & {
  deltaFormatter: (result: Record<string, unknown>) => ToolDelta
  inputValidator: (args: Record<string, unknown>) => string | null
  resultRenderer: (data: unknown) => string[]
  category?: ToolCategory
  allowedCallers?: string[]
  approvalRequired?: boolean
} {
  return {
    definition: buildDefinition(decl),
    deltaFormatter: buildDeltaFormatter(decl.name, decl.resultSchema),
    inputValidator: buildInputValidator(decl.preGates),
    resultRenderer: decl.resultRenderer || buildResultRenderer(decl.name, decl.resultSchema),
    category: decl.category,
    allowedCallers: decl.allowedCallers,
    approvalRequired: decl.approvalRequired,
    execute: decl.execute,
  }
}
