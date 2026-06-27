import type { FunctionTool } from '../types'
import { createSchemaFieldTool } from './create_schema_field'
import { createGateChainTool } from './create_gate_chain'
import { updateGateRuleTool } from './update_gate_rule'
import { validateConfigTool } from './validate_config'
import { generateNormalizerTemplateTool } from './generate_normalizer_template'
import { chatTool } from './chat'
import { writeConfigValueTool, writeConfigValuePreGateRules } from './write_config_value'
import { readConfigValueTool, readConfigValuePreGateRules } from './read_config_value'
import { listDirectoryTool, listDirectoryPreGateRules } from './list_directory'
import { readFileTool, readFilePreGateRules } from './read_file'
import { searchContentTool, searchContentPreGateRules } from './search_content'
import { findFilesTool, findFilesPreGateRules } from './find_files'
import { runTerminalTool, runTerminalPreGateRules } from './run_terminal'
import { readDiagnosticsTool } from './read_diagnostics'
import { listConfigsTool } from './list_configs'
import { askUserTool } from './ask_user'
import { TOOL_PRE_GATE_RULES } from '../preGate'

// ── Pre-Gate 规则注册 ────────────────────────────────────────────────
// 每工具 Pre-Gate 规则在此统一注册，executor 执行前自动校验
TOOL_PRE_GATE_RULES['list_directory'] = listDirectoryPreGateRules
TOOL_PRE_GATE_RULES['read_file'] = readFilePreGateRules
TOOL_PRE_GATE_RULES['search_content'] = searchContentPreGateRules
TOOL_PRE_GATE_RULES['find_files'] = findFilesPreGateRules
TOOL_PRE_GATE_RULES['run_terminal'] = runTerminalPreGateRules
TOOL_PRE_GATE_RULES['write_config_value'] = writeConfigValuePreGateRules
TOOL_PRE_GATE_RULES['read_config_value'] = readConfigValuePreGateRules

/** All white-listed Function Tools available to AI */
export const FUNCTION_TOOLS: FunctionTool[] = [
  createSchemaFieldTool,
  createGateChainTool,
  updateGateRuleTool,
  validateConfigTool,
  generateNormalizerTemplateTool,
  chatTool,
  writeConfigValueTool,
  readConfigValueTool,
  listDirectoryTool,
  readFileTool,
  searchContentTool,
  findFilesTool,
  runTerminalTool,
  readDiagnosticsTool,
  listConfigsTool,
  askUserTool,
]

/** Map tool name -> definition for OpenAI API `tools` param */
export function getToolDefinitions() {
  return FUNCTION_TOOLS.map((t) => t.definition)
}

export { createSchemaFieldTool } from './create_schema_field'
export { createGateChainTool } from './create_gate_chain'
export { updateGateRuleTool } from './update_gate_rule'
export { validateConfigTool } from './validate_config'
export { generateNormalizerTemplateTool } from './generate_normalizer_template'
export { chatTool } from './chat'
export { writeConfigValueTool } from './write_config_value'
export { readConfigValueTool } from './read_config_value'
export { listDirectoryTool } from './list_directory'
export { readFileTool } from './read_file'
export { searchContentTool } from './search_content'
export { findFilesTool } from './find_files'
export { runTerminalTool } from './run_terminal'
export { readDiagnosticsTool } from './read_diagnostics'
export { listConfigsTool } from './list_configs'
export { askUserTool } from './ask_user'
