import type { ValidationResult, ValidationError } from './types'

/**
 * Mock C ABI validation — simulates bs_adapter_parser_parse_bytes return.
 * MVP: in-memory JSON schema check. Production: call C ABI via napi-rs.
 */
export async function validateBlessStarSchema(jsonStr: string): Promise<ValidationResult> {
  let parsed: unknown
  try {
    parsed = JSON.parse(jsonStr)
  } catch {
    return {
      valid: false,
      errors: [{ path: '$', message: 'JSON 语法错误', code: 'PARSE_ERR' }],
    }
  }

  const errors: ValidationError[] = []

  if (!parsed || typeof parsed !== 'object') {
    errors.push({ path: '$', message: '根节点必须是对象', code: 'TYPE_ERR' })
    return { valid: false, errors }
  }

  const obj = parsed as Record<string, unknown>

  // Require version field
  if (typeof obj.version !== 'string') {
    errors.push({ path: '$.version', message: '缺少 version 字段或类型非 string', code: 'REQUIRED_FIELD' })
  }

  // Require fields array if present
  if (obj.fields !== undefined) {
    if (!Array.isArray(obj.fields)) {
      errors.push({ path: '$.fields', message: 'fields 必须是数组', code: 'TYPE_ERR' })
    } else {
      for (let i = 0; i < (obj.fields as unknown[]).length; i++) {
        const f = (obj.fields as unknown[])[i] as Record<string, unknown>
        if (!f.key || typeof f.key !== 'string') {
          errors.push({ path: `$.fields[${i}].key`, message: '每个 field 必须有 string 类型的 key', code: 'REQUIRED_FIELD' })
        }
        if (!f.widget || typeof f.widget !== 'string') {
          errors.push({ path: `$.fields[${i}].widget`, message: '每个 field 必须有 string 类型的 widget', code: 'REQUIRED_FIELD' })
        }
      }
    }
  }

  return { valid: errors.length === 0, errors }
}

export async function validateGateRule(jsonStr: string): Promise<ValidationResult> {
  let parsed: unknown
  try {
    parsed = JSON.parse(jsonStr)
  } catch {
    return {
      valid: false,
      errors: [{ path: '$', message: 'Gate 规则 JSON 语法错误', code: 'PARSE_ERR' }],
    }
  }

  const errors: ValidationError[] = []

  if (!parsed || typeof parsed !== 'object') {
    errors.push({ path: '$', message: 'Gate 规则根节点必须是对象', code: 'TYPE_ERR' })
    return { valid: false, errors }
  }

  const obj = parsed as Record<string, unknown>

  if (!obj.gate_id || typeof obj.gate_id !== 'string') {
    errors.push({ path: '$.gate_id', message: '缺少 gate_id 字段', code: 'REQUIRED_FIELD' })
  }

  if (!obj.scenario || typeof obj.scenario !== 'string') {
    errors.push({ path: '$.scenario', message: '缺少 scenario 字段', code: 'REQUIRED_FIELD' })
  }

  if (!Array.isArray(obj.do)) {
    errors.push({ path: '$.do', message: 'do 必须是数组', code: 'TYPE_ERR' })
  }

  return { valid: errors.length === 0, errors }
}

export function formatValidationErrors(result: ValidationResult): string {
  if (result.valid) return ''
  return result.errors.map((e) => `  [${e.code || 'ERR'}] ${e.path}: ${e.message}`).join('\n')
}
