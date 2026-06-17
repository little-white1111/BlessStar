import type { Block, Generator } from 'blockly'
import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from '../block_registry'

const LOGIC_AND_TYPE = 'bs_logic_and'
const LOGIC_OR_TYPE = 'bs_logic_or'

const logicAndJsonDef = {
  type: LOGIC_AND_TYPE,
  message0: 'A AND B',
  message1: '条件 A %1',
  args1: [{ type: 'input_value', name: 'A' }],
  message2: '条件 B %1',
  args2: [{ type: 'input_value', name: 'B' }],
  colour: 120,
  tooltip: '逻辑与：A 和 B 都满足',
  helpUrl: '',
}

const logicOrJsonDef = {
  type: LOGIC_OR_TYPE,
  message0: 'A OR B',
  message1: '条件 A %1',
  args1: [{ type: 'input_value', name: 'A' }],
  message2: '条件 B %1',
  args2: [{ type: 'input_value', name: 'B' }],
  colour: 120,
  tooltip: '逻辑或：A 或 B 满足其一',
  helpUrl: '',
}

function logicGenerator(block: Block, generator: Generator): string {
  const op = block.type === LOGIC_AND_TYPE ? 'and' : 'or'
  const aVal = generator.valueToCode(block, 'A', 0)
  const bVal = generator.valueToCode(block, 'B', 0)

  const result: any = {
    type: `logic_${op.toUpperCase()}`,
  }

  if (aVal) {
    try { result.A = JSON.parse(aVal) } catch { result.A = aVal }
  }
  if (bVal) {
    try { result.B = JSON.parse(bVal) } catch { result.B = bVal }
  }

  return JSON.stringify(result)
}

function register(): void {
  Blockly.defineBlocksWithJsonArray([logicAndJsonDef, logicOrJsonDef])

  BlessStarBlockRegistry.registerBlockType(LOGIC_AND_TYPE, {
    type: LOGIC_AND_TYPE,
    category: '逻辑',
    colour: 120,
    jsonDef: logicAndJsonDef,
    generator: logicGenerator,
  })

  BlessStarBlockRegistry.registerBlockType(LOGIC_OR_TYPE, {
    type: LOGIC_OR_TYPE,
    category: '逻辑',
    colour: 120,
    jsonDef: logicOrJsonDef,
    generator: logicGenerator,
  })
}

export { LOGIC_AND_TYPE, LOGIC_OR_TYPE, logicAndJsonDef, logicOrJsonDef, logicGenerator, register }
export default register
