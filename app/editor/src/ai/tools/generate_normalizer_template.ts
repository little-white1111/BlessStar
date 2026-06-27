import type { FunctionTool, ToolResult } from '../types'

export const generateNormalizerTemplateTool: FunctionTool = {
  definition: {
    name: 'generate_normalizer_template',
    description: '生成厂商/业务配置归一化器（Normalizer）的 JSON 模板，用于将厂商特有格式映射到统一 Schema',
    parameters: {
      type: 'object',
      properties: {
        vendor_name: {
          type: 'string',
          description: '厂商或业务源名称，如 "yonyou"、"kingdee"、"generic_business"',
        },
        version: {
          type: 'string',
          description: '模板版本号，默认 "1.0"',
        },
        field_count: {
          type: 'number',
          description: '预期的字段数量（可选），用于生成对应数量的占位映射条目',
          default: 3,
        },
      },
      required: ['vendor_name'],
    },
  },

  resultRenderer(data: unknown): string[] {
    const d = data as Record<string, unknown> | undefined
    if (!d) return ['❌ 无数据']
    const template = d.template as Record<string, unknown> | undefined
    const mapping = template?.mapping as Array<unknown> | undefined
    const lines: string[] = ['✅ 已生成归一化模板']
    if (template?.normalizer_id) lines.push(`  ID: ${template.normalizer_id}`)
    if (template?.source_vendor) lines.push(`  厂商: ${template.source_vendor}`)
    if (template?.version) lines.push(`  版本: ${template.version}`)
    if (mapping) lines.push(`  字段映射数: ${mapping.length}`)
    if (d.instructions) lines.push(`  说明: ${d.instructions}`)
    return lines
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const vendorName = String(args.vendor_name || '').trim()
    const version = String(args.version || '1.0')
    const fieldCount = Math.max(1, Math.min(20, Number(args.field_count) || 3))

    if (!vendorName.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/)) {
      return {
        success: false,
        error: `厂商名称 "${vendorName}" 格式无效，须以字母或下划线开头，仅含字母数字下划线`,
      }
    }

    const mapping = Array.from({ length: fieldCount }, (_, i) => ({
      source_field: `source_field_${i + 1}`,
      target_field: `target_field_${i + 1}`,
      type: 'string',
      transform: '',
      required: false,
    }))

    const template = {
      source_vendor: vendorName,
      version,
      normalizer_id: `normalizer_${vendorName}`,
      description: `${vendorName} 配置归一化模板`,
      mapping,
    }

    return {
      success: true,
      data: {
        template,
        templateJson: JSON.stringify(template, null, 2),
        instructions: `请将 mapping 中的 source_field 替换为${vendorName}的原始字段名，target_field 替换为目标 Schema 字段名`,
      },
    }
  },
}
