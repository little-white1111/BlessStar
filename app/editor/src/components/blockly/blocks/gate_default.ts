import type { Block, Generator } from 'blockly'
import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from '../block_registry'

const GATE_DEFAULT_TYPE = 'bs_gate_default'

const gateDefaultJsonDef = {
  type: GATE_DEFAULT_TYPE,
  message0: 'Gate %1 do %2',
  args0: [
    {
      type: 'field_input',
      name: 'GATE_NAME',
      text: 'gate_id',
    },
    {
      type: 'input_statement',
      name: 'DO',
    },
  ],
  message1: '场景策略 %1',
  args1: [
    {
      type: 'field_dropdown',
      name: 'SCENARIO',
      options: [
        ['默认', 'default'],
        ['生产', 'production'],
        ['灾备', 'disaster_recovery'],
        ['审计', 'audit'],
        ['调试', 'debug'],
      ],
    },
  ],
  colour: 220,
  tooltip: '定义一个 Gate 节点，包含执行链',
  helpUrl: '',
}

function gateDefaultGenerator(block: Block, generator: Generator): string {
  const gateName = block.getFieldValue('GATE_NAME') || 'gate_id'
  const scenario = block.getFieldValue('SCENARIO') || 'default'
  const doStatements = generator.statementToCode(block, 'DO')

  return JSON.stringify({
    type: 'gate_default',
    gate_id: gateName,
    scenario,
    do: doStatements ? JSON.parse(doStatements) : [],
  })
}

function register(): void {
  Blockly.defineBlocksWithJsonArray([gateDefaultJsonDef])

  BlessStarBlockRegistry.registerBlockType(GATE_DEFAULT_TYPE, {
    type: GATE_DEFAULT_TYPE,
    category: 'Gate',
    colour: 220,
    jsonDef: gateDefaultJsonDef,
    generator: gateDefaultGenerator,
    deserializer: (json: any) => ({
      GATE_NAME: json.gate_id || 'gate_id',
      SCENARIO: json.scenario || 'default',
    }),
  })
}

export { GATE_DEFAULT_TYPE, gateDefaultJsonDef, gateDefaultGenerator, register }
export default register
