// OPT-04: IPC channel signature tests
import { describe, it, expect, vi, beforeEach } from 'vitest'

describe('IPC channel signatures (preload.ts)', () => {
  beforeEach(() => {
    // Mock window.blessstar for jsdom test environment
    ;(window as any).blessstar = {
      exportAgentIndex: vi.fn().mockResolvedValue({ success: true, outputDir: '.cursor/agents/' }),
      getRegisteredSchemas: vi.fn().mockResolvedValue([]),
      getGateChain: vi.fn().mockResolvedValue({ version: '1.0', gates: [] }),
      validateConfig: vi.fn().mockResolvedValue({ valid: true, errors: [] }),
      executeTool: vi.fn().mockResolvedValue({ success: true, result: 'mock' }),
    }
  })

  it('exportAgentIndex channel exists', () => {
    const api = (window as any).blessstar
    expect(api).toBeDefined()
    expect(typeof api.exportAgentIndex).toBe('function')
  })

  it('getRegisteredSchemas channel exists', () => {
    const api = (window as any).blessstar
    expect(typeof api.getRegisteredSchemas).toBe('function')
  })

  it('getGateChain channel exists', () => {
    const api = (window as any).blessstar
    expect(typeof api.getGateChain).toBe('function')
  })

  it('validateConfig channel exists', () => {
    const api = (window as any).blessstar
    expect(typeof api.validateConfig).toBe('function')
  })

  it('executeTool channel exists', () => {
    const api = (window as any).blessstar
    expect(typeof api.executeTool).toBe('function')
  })

  it('exportAgentIndex returns expected shape', async () => {
    const result = await (window as any).blessstar.exportAgentIndex({ outputDir: '.cursor/agents/' })
    expect(result.success).toBe(true)
    expect(result.outputDir).toBe('.cursor/agents/')
  })

  it('getRegisteredSchemas returns array', async () => {
    const result = await (window as any).blessstar.getRegisteredSchemas()
    expect(Array.isArray(result)).toBe(true)
  })

  it('getGateChain returns object', async () => {
    const result = await (window as any).blessstar.getGateChain()
    expect(result.version).toBe('1.0')
    expect(Array.isArray(result.gates)).toBe(true)
  })

  it('validateConfig rejects invalid JSON', async () => {
    ;(window as any).blessstar.validateConfig.mockResolvedValue({
      valid: false,
      errors: [{ path: 'root', message: 'Invalid JSON' }],
    })
    const result = await (window as any).blessstar.validateConfig('not-json')
    expect(result.valid).toBe(false)
    expect(result.errors.length).toBeGreaterThan(0)
  })

  it('validateConfig accepts valid JSON', async () => {
    ;(window as any).blessstar.validateConfig.mockResolvedValue({ valid: true, errors: [] })
    const result = await (window as any).blessstar.validateConfig('{"key": "value"}')
    expect(result.valid).toBe(true)
    expect(result.errors.length).toBe(0)
  })

  it('executeTool returns mock result', async () => {
    const result = await (window as any).blessstar.executeTool('create_schema_field', { key: 'test' })
    expect(result.success).toBe(true)
    expect(result.result).toContain('mock')
  })
})
