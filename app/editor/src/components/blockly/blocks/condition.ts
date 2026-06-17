import type { Block, Generator } from 'blockly'
import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from '../block_registry'

const CONDITION_TYPE = 'bs_condition'

const conditionJsonDef = {
  type: CONDITION_TYPE,
  message0: 'IF %1 THEN %2 ELSE %3',
  args0: [
    {
      type: 'input_value',
      name: 'IF',
    },
    {
      type: 'input_statement',
      name: 'THEN',
    },
    {
      type: 'input_statement',
      name: 'ELSE',
    },
  ],
  message1: '条件名 %1',
  args1: [
    {
      type: 'field_input',
      name: 'COND_NAME',
      text: 'condition_id',
    },
  ],
  colour: 280,
  tooltip: '条件判断：IF 满足条件则执行 THEN，否则执行 ELSE',
  helpUrl: '',
}

function conditionGenerator(block: Block, generator: Generator): string {
  const condName = block.getFieldValue('COND_NAME') || 'condition_id'
  const ifValue = generator.valueToCode(block, 'IF', 0)
  const thenStatements = generator.statementToCode(block, 'THEN')
  const elseStatements = generator.statementToCode(block, 'ELSE')

  const result: any = {
    type: 'condition',
    condition_id: condName,
  }

  if (ifValue) {
    try { result.if = JSON.parse(ifValue) } catch { result.if = ifValue }
  }

  if (thenStatements) {
    try { result.then = JSON.parse(thenStatements) } catch { result.then = thenStatements }
  }

  if (elseStatements) {
    try { result.else = JSON.parse(elseStatements) } catch { result.else = elseStatements }
  }

  return JSON.stringify(result)
}

function register(): void {
  Blockly.defineBlocksWithJsonArray([conditionJsonDef])

  BlessStarBlockRegistry.registerBlockType(CONDITION_TYPE, {
    type: CONDITION_TYPE,
    category: 'Gate',
    colour: 280,
    jsonDef: conditionJsonDef,
    generator: conditionGenerator,
    deserializer: (json: any) => ({
      COND_NAME: json.condition_id || 'condition_id',
    }),
  })
}

export { CONDITION_TYPE, conditionJsonDef, conditionGenerator, register }
export default register
