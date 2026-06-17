import type { Block, Generator } from 'blockly'
import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from '../block_registry'

const CUSTOM_GATE_TYPE = 'bs_custom_gate'

const customGateJsonDef = {
  type: CUSTOM_GATE_TYPE,
  message0: '自定义 Gate %1 do %2',
  args0: [
    {
      type: 'field_input',
      name: 'GATE_TYPE',
      text: 'custom_type',
    },
    {
      type: 'input_statement',
      name: 'DO',
    },
  ],
  message1: '供应商 %1 版本 %2',
  args1: [
    {
      type: 'field_input',
      name: 'VENDOR',
      text: 'default',
    },
    {
      type: 'field_input',
      name: 'VERSION',
      text: '1.0.0',
    },
  ],
  colour: 20,
  tooltip: '第三方自定义 Gate，可扩展供应商特定逻辑',
  helpUrl: '',
}

function customGateGenerator(block: Block, generator: Generator): string {
  const gateType = block.getFieldValue('GATE_TYPE') || 'custom_type'
  const vendor = block.getFieldValue('VENDOR') || 'default'
  const version = block.getFieldValue('VERSION') || '1.0.0'
  const doStatements = generator.statementToCode(block, 'DO')

  return JSON.stringify({
    type: 'custom_gate',
    gate_type: gateType,
    vendor,
    version,
    do: doStatements ? JSON.parse(doStatements) : [],
  })
}

function register(): void {
  Blockly.defineBlocksWithJsonArray([customGateJsonDef])

  BlessStarBlockRegistry.registerBlockType(CUSTOM_GATE_TYPE, {
    type: CUSTOM_GATE_TYPE,
    category: '供应商',
    colour: 20,
    jsonDef: customGateJsonDef,
    generator: customGateGenerator,
    deserializer: (json: any) => ({
      GATE_TYPE: json.gate_type || 'custom_type',
      VENDOR: json.vendor || 'default',
      VERSION: json.version || '1.0.0',
    }),
  })
}

export { CUSTOM_GATE_TYPE, customGateJsonDef, customGateGenerator, register }
export default register
