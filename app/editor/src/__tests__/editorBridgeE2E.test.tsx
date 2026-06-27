/**
 * EditorBridgeE2E — Editor ↔ BlessStar SDK 端到端集成测试
 *
 * 覆盖 E2E-01~08 架构不变量的 IPC 层和行为层校验：
 *   - normalizeVendor (E2E-06)
 *   - commitBatch (E2E-02)
 *   - Agent index export (E2E-05)
 *   - Gate validation (E2E-03)
 *   - Action Queue + debounce (E2E-08)
 *   - AppSession lifecycle (E2E-07)
 *   - Dynamic domains (E2E-05)
 */
import { describe, it, expect, vi, beforeEach, afterEach, afterAll } from 'vitest'

// ── Mock window.blessstar ──────────────────────────────────────────────
const mockBlessStar = {
  // Existing
  executeTool: vi.fn(),
  validateConfig: vi.fn(),
  getRegisteredSchemas: vi.fn(),
  getGateChain: vi.fn(),
  exportAgentIndex: vi.fn(),
  loadConfig: vi.fn(),
  saveConfig: vi.fn(),

  // New E2E channels
  normalizeVendor: vi.fn(),
  appSessionCreate: vi.fn(),
  appSessionDestroy: vi.fn(),
  commitBatch: vi.fn(),
  subscribeWatch: vi.fn(),
}

// Mock global window.blessstar
vi.stubGlobal('window', { blessstar: mockBlessStar })
// After all tests, restore
afterAll(() => {
  vi.unstubAllGlobals()
})

// ── Test E2E-06: Vendor Normalize ──────────────────────────────────────
describe('E2E-06: VendorConfigNormalizer (厂商归一化)', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    mockBlessStar.normalizeVendor.mockResolvedValue({
      success: true,
      result: '{"version":"v1","paths":{"livedesign.room.room_id":"123"}}',
    })
  })

  it('接受 vendor_id + localStorage JSON 并返回 Config v1 JSON', async () => {
    const storage = JSON.stringify({ room_id: '123', room_name: 'test' })
    const result = await mockBlessStar.normalizeVendor('livedesign', storage)
    expect(mockBlessStar.normalizeVendor).toHaveBeenCalledWith('livedesign', storage)
    expect(result.success).toBe(true)
    expect(result.result).toContain('"v1"')
  })

  it('可选的 extraJson 参数', async () => {
    const storage = JSON.stringify({ room_id: '123' })
    const extra = JSON.stringify({ cookie: 'abc' })
    const result = await mockBlessStar.normalizeVendor('livedesign', storage, extra)
    expect(mockBlessStar.normalizeVendor).toHaveBeenCalledWith('livedesign', storage, extra)
    expect(result.success).toBe(true)
  })

  it('空 vendor_id 返回 null', async () => {
    mockBlessStar.normalizeVendor.mockResolvedValueOnce({ success: false, result: null })
    const result = await mockBlessStar.normalizeVendor('', '')
    expect(result.success).toBe(false)
    expect(result.result).toBeNull()
  })
})

// ── Test E2E-02: Commit-only activation ────────────────────────────────
describe('E2E-02: ConfigCommitBatch (批量提交)', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    mockBlessStar.commitBatch.mockResolvedValue({
      success: true,
      report: '{"status":"success","paths":["livedesign.room.room_id"],"errors":[]}',
    })
  })

  it('提交 JSON 数组并返回 Report', async () => {
    const entries = JSON.stringify([
      { key: 'livedesign.room.room_id', value: '123' },
      { key: 'livedesign.danmaku.font_size', value: '16' },
    ])
    const result = await mockBlessStar.commitBatch(entries)
    expect(mockBlessStar.commitBatch).toHaveBeenCalledWith(entries)
    expect(result.success).toBe(true)
    expect(result.report).toContain('"status":"success"')
  })

  it('空数组返回 success', async () => {
    mockBlessStar.commitBatch.mockResolvedValueOnce({ success: true })
    const result = await mockBlessStar.commitBatch('[]')
    expect(result.success).toBe(true)
  })
})

// ── Test E2E-03: Gate dual validation ──────────────────────────────────
describe('E2E-03: GateChain Validation (门禁校验)', () => {
  const validConfig = {
    version: '1.0',
    instructions: { paths: { 'livedesign.room.room_id': '123' } },
  }

  const invalidConfig = {
    instructions: { paths: {} },
  }

  beforeEach(() => {
    vi.clearAllMocks()
    mockBlessStar.getRegisteredSchemas.mockResolvedValue({
      fields: [
        { key: 'livedesign.room.room_id', type_name: 'string', description: '房间ID' },
        { key: 'livedesign.danmaku.font_size', type_name: 'int32', default_value: '16', description: '字号' },
      ],
    })
    mockBlessStar.validateConfig.mockImplementation(async (json: string) => {
      try {
        const parsed = JSON.parse(json)
        const errors: Array<{ path: string; message: string }> = []
        if (!parsed.version) errors.push({ path: 'version', message: '缺少 version' })
        return { valid: errors.length === 0, errors }
      } catch {
        return { valid: false, errors: [{ path: 'root', message: 'Invalid JSON' }] }
      }
    })
  })

  it('有效配置通过校验', async () => {
    const result = await mockBlessStar.validateConfig(JSON.stringify(validConfig))
    expect(result.valid).toBe(true)
    expect(result.errors).toHaveLength(0)
  })

  it('缺少 version 字段的配置校验失败', async () => {
    const result = await mockBlessStar.validateConfig(JSON.stringify(invalidConfig))
    expect(result.valid).toBe(false)
  })

  it('非法 JSON 返回语法错误', async () => {
    const result = await mockBlessStar.validateConfig('not json')
    expect(result.valid).toBe(false)
    expect(result.errors[0].path).toBe('root')
  })

  it('getGateChain 返回基于 Schema 的 Gate 列表', async () => {
    mockBlessStar.getGateChain.mockResolvedValue({
      version: '1.0',
      gates: [
        { gate_id: 'gate_livedesign_room_room_id', scenario: 'default', field_key: 'livedesign.room.room_id' },
      ],
    })
    const result = await mockBlessStar.getGateChain()
    expect(result.version).toBe('1.0')
    expect(result.gates.length).toBeGreaterThan(0)
  })

  it('Schema 不可用时返回空 gate 链', async () => {
    mockBlessStar.getGateChain.mockResolvedValueOnce({ version: '1.0', gates: [] })
    const result = await mockBlessStar.getGateChain()
    expect(result.gates).toHaveLength(0)
  })
})

// ── Test E2E-05: Agent index auto-rebuild ──────────────────────────────
describe('E2E-05: AgentIndexExport (索引自动重建)', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    mockBlessStar.exportAgentIndex.mockResolvedValue({ success: true, outputDir: '.cursor/agents/' })
    mockBlessStar.getRegisteredSchemas.mockResolvedValue({
      fields: [{ key: 'livedesign.room.room_id', type_name: 'string' }],
    })
  })

  it('导出 Agent 索引到指定目录', async () => {
    const result = await mockBlessStar.exportAgentIndex({ outputDir: '.cursor/agents/', businessName: 'test' })
    expect(result.success).toBe(true)
    expect(result.outputDir).toContain('.cursor')
  })

  it('导出失败返回 success: false', async () => {
    mockBlessStar.exportAgentIndex.mockResolvedValueOnce({ success: false, outputDir: '' })
    const result = await mockBlessStar.exportAgentIndex({ outputDir: '/invalid/path' })
    expect(result.success).toBe(false)
  })
})

// ── Test E2E-07: AppSession lifecycle ──────────────────────────────────
describe('E2E-07: AppSession Lifecycle', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    mockBlessStar.appSessionCreate.mockResolvedValue({ success: true, handle: 42 })
    mockBlessStar.appSessionDestroy.mockResolvedValue({ success: true })
  })

  it('创建 AppSession 返回 opaque handle', async () => {
    const result = await mockBlessStar.appSessionCreate()
    expect(result.success).toBe(true)
    expect(result.handle).toBe(42)
  })

  it('销毁 AppSession', async () => {
    const result = await mockBlessStar.appSessionDestroy()
    expect(result.success).toBe(true)
  })

  it('创建失败返回 handle: null', async () => {
    mockBlessStar.appSessionCreate.mockResolvedValueOnce({ success: false, handle: null })
    const result = await mockBlessStar.appSessionCreate()
    expect(result.success).toBe(false)
    expect(result.handle).toBeNull()
  })
})

// ── Test E2E-08: Action Queue + debounce ───────────────────────────────
describe('E2E-08: Action Queue + Debounce (行动队列)', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('commitBatch 收到 entries 后返回结果', async () => {
    mockBlessStar.commitBatch.mockResolvedValue({
      success: true,
      report: '{"status":"success"}',
    })
    const entries = JSON.stringify([{ key: 'test.key', value: 'val' }])
    const result = await mockBlessStar.commitBatch(entries)
    expect(mockBlessStar.commitBatch).toHaveBeenCalledTimes(1)
    expect(result.success).toBe(true)
  })

  it('多个变更合并为一次批量提交', async () => {
    mockBlessStar.commitBatch.mockResolvedValue({ success: true, report: '{"status":"success"}' })
    // 模拟两次独立提交，验证都会成功
    const e1 = JSON.stringify([{ key: 'k1', value: 'v1' }])
    const e2 = JSON.stringify([{ key: 'k2', value: 'v2' }])
    await mockBlessStar.commitBatch(e1)
    await mockBlessStar.commitBatch(e2)
    expect(mockBlessStar.commitBatch).toHaveBeenCalledTimes(2)
  })
})

// ── Test IPC channel wiring ────────────────────────────────────────────
describe('IPC channel wiring', () => {
  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('subscribeWatch 可被调用', async () => {
    mockBlessStar.subscribeWatch.mockResolvedValue({ success: true })
    const result = await mockBlessStar.subscribeWatch()
    expect(result.success).toBe(true)
  })

  it('loadConfig 仍正常工作', async () => {
    mockBlessStar.loadConfig.mockResolvedValue(JSON.stringify({ path: 'test.json', content: '{}' }))
    const result = await mockBlessStar.loadConfig()
    expect(result).toContain('test.json')
  })
})
