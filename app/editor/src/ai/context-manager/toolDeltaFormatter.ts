import type { ToolDelta } from './contextBuilder'
import { findTool } from '../executor'

/**
 * 根据 tool name 和 result 压缩为单行摘要（< 50 tokens）。
 *
 * 优先使用 Tool Declaration 中通过 createTool 工厂内置的 deltaFormatter
 * （对应缺口二：Tool Result Schema 声明式格式化），
 * 未迁移的工具回退到 legacy switch/case 分支。
 */
export function buildToolDelta(toolName: string, result: unknown): ToolDelta {
  if (result === null || result === undefined) {
    return { summary: '❌ 操作失败：空结果' }
  }

  // ── 优先：使用 createTool 工厂内置的 deltaFormatter ──
  // 对应缺口二（Tool Result Schema 声明式格式化），已迁移的工具走此路径
  const tool = findTool(toolName) as Record<string, unknown> | undefined
  if (tool?.deltaFormatter && typeof tool.deltaFormatter === 'function') {
    return (tool.deltaFormatter as (r: unknown) => ToolDelta)(result)
  }

  // ── 回退：legacy switch/case ──

  switch (toolName) {
    case 'create_schema_field': {
      const r = result as Record<string, unknown>
      if (r.success === false) {
        return { summary: `❌ 创建字段失败: ${r.error || '未知错误'}` }
      }
      // 尝试从不同层级提取 field 信息
      const extract = (obj: Record<string, unknown> | undefined): string | null => {
        if (!obj) return null
        if (obj.key) return obj.key as string
        if (obj.field_key) return obj.field_key as string
        return null
      }
      const data = r.data as Record<string, unknown> | undefined
      const nestedField = data?.field as Record<string, unknown> | undefined
      const topField = r.field as Record<string, unknown> | undefined
      const field = nestedField || topField || data || r
      const key = extract(field as Record<string, unknown>) || extract(r as Record<string, unknown>)
      if (key) {
        const f = field as Record<string, unknown>
        return {
          summary: `✅ 已创建字段: ${key} (${String(f.type || f.field_type || 'str')}, ${f.required ? 'required' : 'optional'}, widget=${String(f.widget || 'input')})`,
        }
      }
      return { summary: '✅ 创建字段成功' }
    }

    case 'update_gate_rule': {
      const r = result as Record<string, unknown>
      if (!r.success && r.success !== undefined) {
        return { summary: `❌ 更新 Gate 失败: ${r.error || '未知错误'}` }
      }
      const gateId = r.gate_id || (r.data as Record<string, unknown> | undefined)?.gate_id || ''
      return { summary: `✅ 已更新 Gate 规则: ${gateId}` }
    }

    case 'validate_config': {
      const r = result as Record<string, unknown>
      if (r.success || r.valid) {
        const errors = (r.errors as unknown[])?.length ?? 0
        return { summary: `✅ 校验通过，${errors} 个错误` }
      }
      const firstErr = extractFirstError(r)
      return { summary: `❌ 校验失败: ${firstErr}` }
    }

    case 'chat': {
      const r = result as Record<string, unknown>
      if (r.success === false) {
        return { summary: `❌ chat: ${r.error || '未知错误'}` }
      }
      const data = (r.data || r) as Record<string, unknown>
      const reply = String(data.reply || '').slice(0, 50)
      return { summary: reply || '💬 已回复' }
    }

    case 'generate_normalizer_template': {
      const r = result as Record<string, unknown>
      if (r.success === false) {
        return { summary: `❌ 生成失败: ${r.error || '未知错误'}` }
      }
      const data = (r.data || r) as Record<string, unknown>
      const template = data.template as Record<string, unknown> | undefined
      const name = template?.source_vendor || template?.name || data.name || ''
      const mappings = data.mapping ? (data.mapping as unknown[]).length
        : template?.mapping ? (template.mapping as unknown[]).length
        : 0
      return { summary: `✅ 已生成归一化模板: ${name}${mappings ? `，${mappings} 个映射` : ''}` }
    }

    case 'read_config_value': {
      const r = result as Record<string, unknown>
      if (r.success) {
        const data = r.data as Record<string, unknown> | undefined
        if (data) {
          const key = data.key as string
          const val = data.value
          if (val === null || val === '') {
            return { summary: `⚠️ ${key} 未设置值，当前为空` }
          }
          return { summary: `📖 ${key} = "${val}"` }
        }
        return { summary: '✅ 读取成功' }
      }
      return { summary: `❌ 读取失败: ${(r.error as string) || '未知错误'}` }
    }

    default: {
      const r = result as Record<string, unknown>
      if (r.success) {
        return { summary: '✅ 操作成功' }
      }
      if (r.success === false) {
        return { summary: `❌ 操作失败: ${r.error || '未知错误'}` }
      }
      return { summary: '✅ 操作成功' }
    }
  }
}

function extractFirstError(r: Record<string, unknown>): string {
  // Check common error patterns
  if (r.error) return String(r.error).slice(0, 80)

  const errors = r.errors as unknown[] | undefined
  if (errors && errors.length > 0) {
    const first = errors[0] as Record<string, unknown>
    return String(first.message || first.error || JSON.stringify(first)).slice(0, 80)
  }

  if (r.message) return String(r.message).slice(0, 80)

  return '配置不符合规范'
}
