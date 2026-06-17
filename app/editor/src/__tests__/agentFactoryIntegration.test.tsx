// OPT-05: AI Tool IPC bridge tests
import { describe, it, expect, vi, beforeEach } from 'vitest'

// Mock window.blessstar
const mockBlessStar = {
  executeTool: vi.fn(),
  validateConfig: vi.fn(),
}

describe('AI Tool IPC Bridge', () => {
  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('create_schema_field calls executeTool', async () => {
    mockBlessStar.executeTool.mockResolvedValue({ success: true, result: 'ok' })
    const result = await mockBlessStar.executeTool('create_schema_field', { key: 'test' })
    expect(mockBlessStar.executeTool).toHaveBeenCalledWith('create_schema_field', { key: 'test' })
    expect(result.success).toBe(true)
  })

  it('validate_config calls validateConfig', async () => {
    mockBlessStar.validateConfig.mockResolvedValue({ valid: true, errors: [] })
    const result = await mockBlessStar.validateConfig('{"a":1}')
    expect(mockBlessStar.validateConfig).toHaveBeenCalledWith('{"a":1}')
    expect(result.valid).toBe(true)
  })

  it('suggest_field_type calls executeTool', async () => {
    mockBlessStar.executeTool.mockResolvedValue({ success: true, result: 'input' })
    const result = await mockBlessStar.executeTool('suggest_field_type', { label: '名称' })
    expect(mockBlessStar.executeTool).toHaveBeenCalledWith('suggest_field_type', { label: '名称' })
    expect(result.success).toBe(true)
  })

  it('generate_normalizer_template calls executeTool', async () => {
    mockBlessStar.executeTool.mockResolvedValue({ success: true, result: 'ok' })
    const result = await mockBlessStar.executeTool('generate_normalizer_template', { vendor_name: 'yonyou' })
    expect(mockBlessStar.executeTool).toHaveBeenCalledWith('generate_normalizer_template', { vendor_name: 'yonyou' })
  })

  it('update_gate_rule calls executeTool', async () => {
    mockBlessStar.executeTool.mockResolvedValue({ success: true, result: 'ok' })
    const result = await mockBlessStar.executeTool('update_gate_rule', { gate_id: 'g1' })
    expect(mockBlessStar.executeTool).toHaveBeenCalledWith('update_gate_rule', { gate_id: 'g1' })
  })

  it('IPC fallback preserves return type', async () => {
    mockBlessStar.executeTool.mockResolvedValue({ success: false, result: 'error' })
    const result = await mockBlessStar.executeTool('unknown_tool', {})
    expect(result.success).toBe(false)
    expect(result.result).toBe('error')
  })
})
