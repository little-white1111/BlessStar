import type { Block, Generator } from 'blockly'
import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from '../block_registry'

const POLICY_ATTR_TYPE = 'bs_policy_attr'

const policyAttrJsonDef = {
  type: POLICY_ATTR_TYPE,
  message0: '策略属性 %1 = %2',
  args0: [
    {
      type: 'field_input',
      name: 'ATTR_KEY',
      text: 'key',
    },
    {
      type: 'field_input',
      name: 'ATTR_VALUE',
      text: 'value',
    },
  ],
  colour: 300,
  tooltip: '设置 ScenarioPolicy 属性键值对',
  helpUrl: '',
}

function policyAttrGenerator(block: Block, generator: Generator): string {
  const key = block.getFieldValue('ATTR_KEY') || 'key'
  const value = block.getFieldValue('ATTR_VALUE') || ''

  return JSON.stringify({
    type: 'policy_attr',
    key,
    value,
  })
}

function register(): void {
  Blockly.defineBlocksWithJsonArray([policyAttrJsonDef])

  BlessStarBlockRegistry.registerBlockType(POLICY_ATTR_TYPE, {
    type: POLICY_ATTR_TYPE,
    category: '策略',
    colour: 300,
    jsonDef: policyAttrJsonDef,
    generator: policyAttrGenerator,
    deserializer: (json: any) => ({
      ATTR_KEY: json.key || 'key',
      ATTR_VALUE: json.value || '',
    }),
  })
}

export { POLICY_ATTR_TYPE, policyAttrJsonDef, policyAttrGenerator, register }
export default register
