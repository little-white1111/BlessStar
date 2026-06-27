import { describe, it, expect } from 'vitest'
import { readFileTool, readFilePreGateRules } from '../read_file'
import { searchContentTool } from '../search_content'
import { findFilesTool } from '../find_files'
import { runTerminalTool, validateCommandAllowed } from '../run_terminal'
import { readDiagnosticsTool } from '../read_diagnostics'
import { FUNCTION_TOOLS } from '../index'

describe('read_file — 读取文件工具', () => {
  it('使用 createTool 工厂创建', () => {
    expect(readFileTool.definition.name).toBe('read_file')
    expect(readFileTool.category).toBe('retrieval')
  })

  it('Pre-Gate 规则存在', () => {
    expect(readFilePreGateRules.length).toBeGreaterThanOrEqual(2)
    expect(readFilePreGateRules[0].field).toBe('path')
  })
})

describe('search_content — 搜索内容工具', () => {
  it('使用 createTool 工厂创建', () => {
    expect(searchContentTool.definition.name).toBe('search_content')
    expect(searchContentTool.category).toBe('retrieval')
  })

  it('必填参数: path 和 pattern', () => {
    const params = searchContentTool.definition.parameters.properties as Record<string, unknown>
    expect(params).toHaveProperty('path')
    expect(params).toHaveProperty('pattern')
  })
})

describe('find_files — 查找文件工具', () => {
  it('使用 createTool 工厂创建', () => {
    expect(findFilesTool.definition.name).toBe('find_files')
    expect(findFilesTool.category).toBe('retrieval')
  })

  it('必填参数: path 和 pattern', () => {
    const params = findFilesTool.definition.parameters.properties as Record<string, unknown>
    expect(params).toHaveProperty('path')
    expect(params).toHaveProperty('pattern')
  })
})

describe('run_terminal — 受限终端工具', () => {
  it('使用 createTool 工厂创建', () => {
    expect(runTerminalTool.definition.name).toBe('run_terminal')
    expect(runTerminalTool.category).toBe('terminal')
    expect(runTerminalTool.approvalRequired).toBe(true)
  })

  it('validateCommandAllowed 拒绝写命令', () => {
    expect(validateCommandAllowed('del /f config.json')).toContain('不在白名单中')
    expect(validateCommandAllowed('rm -rf /')).toContain('不在白名单中')
  })

  it('validateCommandAllowed 允许读命令', () => {
    expect(validateCommandAllowed('dir /b')).toBeNull()
    expect(validateCommandAllowed('tree /f')).toBeNull()
  })
})

describe('read_diagnostics — 诊断信息工具', () => {
  it('使用 createTool 工厂创建', () => {
    expect(readDiagnosticsTool.definition.name).toBe('read_diagnostics')
    expect(readDiagnosticsTool.category).toBe('execution')
  })
})

describe('工具总数', () => {
  it('FUNCTION_TOOLS 现在有 13 个', () => {
    const toolNames = FUNCTION_TOOLS.map((t) => t.definition.name)
    expect(toolNames).toContain('read_file')
    expect(toolNames).toContain('search_content')
    expect(toolNames).toContain('find_files')
    expect(toolNames).toContain('run_terminal')
    expect(toolNames).toContain('read_diagnostics')
    expect(FUNCTION_TOOLS.length).toBeGreaterThanOrEqual(13)
  })
})
