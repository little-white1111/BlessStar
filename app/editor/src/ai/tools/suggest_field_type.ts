import type { FunctionTool, ToolResult } from '../types'

const LABEL_TO_WIDGET: Record<string, string> = {
  开关: 'checkbox',
  启: 'checkbox',
  性别: 'radio',
  名称: 'input',
  名字: 'input',
  标题: 'input',
  描述: 'textarea',
  备注: 'textarea',
  数量: 'number',
  个数: 'number',
  大小: 'number',
  价格: 'number',
  金额: 'number',
  选择: 'select',
  选项: 'select',
  类型: 'select',
  分类: 'select',
  状态: 'select',
  邮箱: 'input',
  地址: 'input',
  电话: 'input',
  手机: 'input',
  密码: 'input',
  URL: 'input',
  链接: 'input',
  日期: 'input',
  时间: 'input',
  颜色: 'input',
  图片: 'input',
  文件: 'input',
}

const WIDGET_HINTS: Record<string, string> = {
  input: '单行文本输入框，适用于短文本',
  select: '下拉选择框，适用于枚举值',
  checkbox: '复选框，适用于开关/布尔值',
  radio: '单选按钮组，适用于少量互斥选项（2-5 个）',
  number: '数字输入框，适用于数值',
  textarea: '多行文本输入框，适用于较长文本',
  group: '字段分组，用于组织子字段',
  repeatable: '可重复字段组，适用于列表/数组',
}

export const suggestFieldTypeTool: FunctionTool = {
  definition: {
    name: 'suggest_field_type',
    description: '根据字段标签或名称，推荐最合适的控件类型（widget），并给出备选建议',
    parameters: {
      type: 'object',
      properties: {
        label: {
          type: 'string',
          description: '字段标签或名称文本，用于语义匹配控件类型',
        },
        context: {
          type: 'string',
          description: '额外上下文说明（可选），如值的范围、选项列表等',
        },
      },
      required: ['label'],
    },
  },

  async execute(args: Record<string, unknown>): Promise<ToolResult> {
    const label = String(args.label || '').trim()
    const context = args.context ? String(args.context) : ''

    if (!label) {
      return { success: false, error: 'label 不能为空' }
    }

    let primaryWidget = 'input'

    for (const [keyword, widget] of Object.entries(LABEL_TO_WIDGET)) {
      if (label.includes(keyword)) {
        primaryWidget = widget
        break
      }
    }

    // If context mentions specific options, prefer radio/select
    const hasOptions = context.includes('选项') || context.includes('枚举') || context.includes('类型')
    if (hasOptions && label.includes('类型') || hasOptions && label.includes('状态')) {
      primaryWidget = 'select'
    }

    const suggestions: Array<{ widget: string; reason: string }> = [
      { widget: primaryWidget, reason: WIDGET_HINTS[primaryWidget] || '通用输入' },
    ]

    // Add one alternative
    const altWidget = primaryWidget === 'input' ? 'textarea' : 'input'
    suggestions.push({ widget: altWidget, reason: WIDGET_HINTS[altWidget] || '备选' })

    return {
      success: true,
      data: {
        primary: primaryWidget,
        suggestions,
        reasoning: `基于字段标签"${label}"${context ? `和上下文"${context}"` : ''}的语义分析`,
      },
    }
  },
}
