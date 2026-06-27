import type { Block, Generator } from 'blockly'
import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from '../block_registry'

const META_RULE_TYPE = 'bs_meta_rule'

const metaRuleJsonDef = {
  type: META_RULE_TYPE,
  message0: '元规则: 字段 %1 检查 %2 值 %3',
  args0: [
    {
      type: 'field_input',
      name: 'FIELD',
      text: 'metadata.field',
    },
    {
      type: 'field_dropdown',
      name: 'OPERATOR',
      options: [
        ['等于', 'eq'],
        ['不等于', 'ne'],
        ['包含', 'contains'],
        ['匹配正则', 'regex'],
        ['存在', 'exists'],
        ['不存在', 'not_exists'],
      ],
    },
    {
      type: 'field_input',
      name: 'VALUE',
      text: '',
    },
  ],
  colour: 160,
  tooltip: '元数据字段检查规则',
  helpUrl: '',
}

function metaRuleGenerator(block: Block, _generator: Generator): string {
  const field = block.getFieldValue('FIELD') || 'metadata.field'
  const operator = block.getFieldValue('OPERATOR') || 'eq'
  const value = block.getFieldValue('VALUE') || ''

  return JSON.stringify({
    type: 'meta_rule',
    field,
    operator,
    value,
  })
}

function register(): void {
  Blockly.defineBlocksWithJsonArray([metaRuleJsonDef])

  BlessStarBlockRegistry.registerBlockType(META_RULE_TYPE, {
    type: META_RULE_TYPE,
    category: 'Gate',
    colour: 160,
    jsonDef: metaRuleJsonDef,
    generator: metaRuleGenerator,
    deserializer: (json: any) => ({
      FIELD: json.field || 'metadata.field',
      OPERATOR: json.operator || 'eq',
      VALUE: json.value || '',
    }),
  })
}

export { META_RULE_TYPE, metaRuleJsonDef, metaRuleGenerator, register }
export default register
