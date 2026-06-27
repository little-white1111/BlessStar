import { describe, it, expect, beforeEach } from 'vitest'
import { ExecutionTraceManager, ToolCallRegistry } from '../executionTrace'

// ── ExecutionTraceManager ────────────────────────────────────────────

describe('ExecutionTraceManager — 工具执行轨迹 DAG', () => {
  let trace: ExecutionTraceManager

  beforeEach(() => {
    trace = new ExecutionTraceManager()
  })

  it('初始状态：空 trace', () => {
    expect(trace.serialize()).toBe('')
    expect(trace.getCurrentTrace().nodes).toHaveLength(0)
  })

  it('newRound 递增轮次', () => {
    expect(trace.getCurrentTrace().round).toBe(0)
    trace.newRound()
    expect(trace.getCurrentTrace().round).toBe(1)
    trace.newRound()
    expect(trace.getCurrentTrace().round).toBe(2)
  })

  it('addNode 添加一个工具调用节点', () => {
    trace.newRound()
    const node = trace.addNode({
      toolName: 'list_directory',
      input: { path: '/models' },
      outputSummary: '📂 已找到 3 个模型',
    })
    expect(node.callId).toContain('call_')
    expect(node.toolName).toBe('list_directory')
    expect(node.dependsOn).toEqual([])

    const current = trace.getCurrentTrace()
    expect(current.round).toBe(1)
    expect(current.nodes).toHaveLength(1)
  })

  it('addNode 支持依赖关系', () => {
    trace.newRound()
    const n1 = trace.addNode({
      toolName: 'read_file',
      input: { path: '/models/config.json' },
      outputSummary: '已读取 config.json',
    })
    const n2 = trace.addNode({
      toolName: 'write_config_value',
      input: { key: 'room_id', value: '123' },
      outputSummary: '✅ 已修改 room_id = 123',
      dependsOn: [n1.callId],
    })
    expect(n2.dependsOn).toEqual([n1.callId])
  })

  it('serialize 输出可读格式', () => {
    trace.newRound()
    trace.addNode({
      toolName: 'list_directory',
      input: { path: '/models' },
      outputSummary: '📂 已找到 3 个模型',
    })
    trace.addNode({
      toolName: 'read_file',
      input: { path: '/models/model.json' },
      outputSummary: '已读取 model.json',
      dependsOn: [],
    })

    const text = trace.serialize()
    expect(text).toContain('工具执行轨迹:')
    expect(text).toContain('list_directory')
    expect(text).toContain('read_file')
  })

  it('serialize 包含依赖信息', () => {
    trace.newRound()
    const n1 = trace.addNode({
      toolName: 'list_directory',
      input: { path: '/' },
      outputSummary: '已列出根目录',
    })
    trace.addNode({
      toolName: 'read_file',
      input: { path: '/config.json' },
      outputSummary: '已读取 config.json',
      dependsOn: [n1.callId],
    })
    const text = trace.serialize()
    expect(text).toContain('依赖:')
  })

  it('reset 保留最近轮次', () => {
    trace.newRound()
    for (let i = 0; i < 5; i++) {
      trace.addNode({ toolName: 'tool_a', input: {}, outputSummary: `结果 ${i}` })
    }
    trace.reset(1)
    expect(trace.getCurrentTrace().nodes.length).toBeLessThanOrEqual(10)
  })
})

// ── ToolCallRegistry ─────────────────────────────────────────────────

describe('ToolCallRegistry — 工具调用注册表', () => {
  let registry: ToolCallRegistry

  beforeEach(() => {
    registry = new ToolCallRegistry()
  })

  it('record 记录一条工具调用', () => {
    const rec = registry.record('call_rf_01', 'read_file', '已读取 config.json', 'success')
    expect(rec.callId).toBe('call_rf_01')
    expect(rec.toolName).toBe('read_file')
    expect(rec.status).toBe('success')
    expect(rec.timestamp).toBeGreaterThan(0)
    expect(rec.outputHash).toBeTruthy()
  })

  it('findByCallId 按 callId 查找', () => {
    registry.record('call_ld_01', 'list_directory', '📂 已找到 3 个模型', 'success')
    const found = registry.findByCallId('call_ld_01')
    expect(found).not.toBeNull()
    expect(found!.toolName).toBe('list_directory')

    const notFound = registry.findByCallId('call_xx_99')
    expect(notFound).toBeNull()
  })

  it('verifyOutput 验证输出一致性', () => {
    registry.record('call_wr_01', 'write_config_value', '✅ 已写入', 'success')
    expect(registry.verifyOutput('call_wr_01', '✅ 已写入')).toBe(true)
    expect(registry.verifyOutput('call_wr_01', '❌ 其他内容')).toBe(false)
  })

  it('verifyAIClaims 检测无回执的工具声称', () => {
    const result = registry.verifyAIClaims('已列出 3 个文件')
    expect(result.verified).toBe(false)
    expect(result.reason).toContain('无回执编号')
  })

  it('verifyAIClaims 验证 callId 存在', () => {
    registry.record('call_ld_01', 'list_directory', '📂 已找到 3 个模型', 'success')
    const result = registry.verifyAIClaims('[call_ld_01] 已列出 3 个模型')
    expect(result.verified).toBe(true)
  })

  it('verifyAIClaims 验证不存在的 callId', () => {
    const result = registry.verifyAIClaims('[call_xx_99] 已列出 3 个模型')
    expect(result.verified).toBe(false)
    expect(result.reason).toContain('不存在')
  })
})
