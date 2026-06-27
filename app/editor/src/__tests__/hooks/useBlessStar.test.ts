import { renderHook, act } from '@testing-library/react'
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { useBlessStar } from '../../hooks/useBlessStar'

const mockLoadConfig = vi.fn()
const mockSaveConfig = vi.fn()
const mockSaveToPath = vi.fn()
const mockSchemaToUidl = vi.fn()

beforeEach(() => {
  vi.clearAllMocks()
  window.blessstar = {
    loadConfig: mockLoadConfig,
    saveConfig: mockSaveConfig,
    saveToPath: mockSaveToPath,
    schemaToUidl: mockSchemaToUidl,
    onConfigChanged: vi.fn(),
    onMenuOpen: vi.fn(),
    onMenuSave: vi.fn(),
    onMenuSaveAs: vi.fn(),
    // Agent Factory (OPT-04)
    exportAgentIndex: vi.fn(),
    getRegisteredSchemas: vi.fn(),
    getGateChain: vi.fn(),
    validateConfig: vi.fn(),
    executeTool: vi.fn(),
    // AI
    aiComplete: vi.fn(),
    ollamaListModels: vi.fn(),
    aiChat: vi.fn(),
    // E2E Editor Bridge
    normalizeVendor: vi.fn(),
    appSessionCreate: vi.fn(),
    appSessionDestroy: vi.fn(),
    commitBatch: vi.fn(),
    subscribeWatch: vi.fn(),
  } as any
})

describe('useBlessStar', () => {
  it('loadConfig returns parsed config', async () => {
    mockLoadConfig.mockResolvedValue(
      JSON.stringify({ path: '/test.json', content: '{"key":"val"}' }),
    )
    const { result } = renderHook(() => useBlessStar())

    let config
    await act(async () => {
      config = await result.current.loadConfig()
    })

    expect(config).toEqual({ path: '/test.json', content: '{"key":"val"}' })
    expect(result.current.loading).toBe(false)
    expect(result.current.error).toBeNull()
  })

  it('loadConfig returns null when no file selected', async () => {
    mockLoadConfig.mockResolvedValue(null)
    const { result } = renderHook(() => useBlessStar())

    let config
    await act(async () => {
      config = await result.current.loadConfig()
    })

    expect(config).toBeNull()
  })

  it('loadConfig sets error on exception', async () => {
    mockLoadConfig.mockRejectedValue(new Error('IPC error'))
    const { result } = renderHook(() => useBlessStar())

    await act(async () => {
      await result.current.loadConfig()
    })

    expect(result.current.error).toBe('IPC error')
  })

  it('saveConfig returns true on success', async () => {
    mockSaveConfig.mockResolvedValue(true)
    const { result } = renderHook(() => useBlessStar())

    let ok
    await act(async () => {
      ok = await result.current.saveConfig('{}')
    })
    expect(ok).toBe(true)
  })

  it('saveConfig returns false on exception', async () => {
    mockSaveConfig.mockRejectedValue(new Error('save error'))
    const { result } = renderHook(() => useBlessStar())

    await act(async () => {
      await result.current.saveConfig('{}')
    })
    expect(result.current.error).toBe('save error')
  })

  it('saveToPath calls IPC and returns result', async () => {
    mockSaveToPath.mockResolvedValue(true)
    const { result } = renderHook(() => useBlessStar())

    let ok
    await act(async () => {
      ok = await result.current.saveToPath('/path.json', '{}')
    })
    expect(ok).toBe(true)
    expect(mockSaveToPath).toHaveBeenCalledWith('/path.json', '{}')
  })

  it('schemaToUidl returns parsed UIDL', async () => {
    mockSchemaToUidl.mockResolvedValue('{"render_type":"dynamic_form"}')
    const { result } = renderHook(() => useBlessStar())

    let uidl
    await act(async () => {
      uidl = await result.current.schemaToUidl()
    })
    expect(uidl).toBe('{"render_type":"dynamic_form"}')
  })

  it('schemaToUidl returns null on failure', async () => {
    mockSchemaToUidl.mockRejectedValue(new Error('no schema'))
    const { result } = renderHook(() => useBlessStar())

    await act(async () => {
      await result.current.schemaToUidl()
    })
    expect(result.current.error).toBe('no schema')
  })

  it('clearError resets error', async () => {
    mockLoadConfig.mockRejectedValue(new Error('err'))
    const { result } = renderHook(() => useBlessStar())

    await act(async () => {
      await result.current.loadConfig()
    })
    expect(result.current.error).toBe('err')

    act(() => {
      result.current.clearError()
    })
    expect(result.current.error).toBeNull()
  })
})
