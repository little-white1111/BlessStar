import type { FunctionTool } from '../types'
import { createSchemaFieldTool } from './create_schema_field'
import { updateGateRuleTool } from './update_gate_rule'
import { validateConfigTool } from './validate_config'
import { suggestFieldTypeTool } from './suggest_field_type'
import { generateNormalizerTemplateTool } from './generate_normalizer_template'

/** All 5 white-listed Function Tools available to AI */
export const FUNCTION_TOOLS: FunctionTool[] = [
  createSchemaFieldTool,
  updateGateRuleTool,
  validateConfigTool,
  suggestFieldTypeTool,
  generateNormalizerTemplateTool,
]

/** Map tool name -> definition for OpenAI API `tools` param */
export function getToolDefinitions() {
  return FUNCTION_TOOLS.map((t) => t.definition)
}

export { createSchemaFieldTool } from './create_schema_field'
export { updateGateRuleTool } from './update_gate_rule'
export { validateConfigTool } from './validate_config'
export { suggestFieldTypeTool } from './suggest_field_type'
export { generateNormalizerTemplateTool } from './generate_normalizer_template'
