/**
 * create_gate_chain AI tool 测试
 */
import { describe, it, expect, beforeAll } from 'vitest'
import { createGateChainTool } from '../create_gate_chain'

beforeAll(() => {
  // ── DAY38-02：模拟 window.blessstar.executeTool（原生 gate 路由）──
  ;(globalThis as Record<string, unknown>).window = {
    blessstar: {
      registerGate: async (_gateType: string, _ruleJson: string) => ({
        success: true,
      }),
      executeTool: async (params: any) => {
        if (params.tool === 'create_gate_chain') {
          return JSON.stringify({ success: true, node_ptr: '0xmock' })
        }
        if (params.tool === 'gate_map_upsert') {
          return JSON.stringify({ success: true })
        }
        return { success: true }
      },
    },
  }
})

describe('createGateChainTool — definition', () => {
  it('应定义 name 为 create_gate_chain', () => {
    expect(createGateChainTool.definition.name).toBe('create_gate_chain')
  })

  it('应定义 description', () => {
    expect(createGateChainTool.definition.description).toBeTruthy()
    expect(createGateChainTool.definition.description.length).toBeGreaterThan(10)
  })

  it('应定义 required 参数: gate_id', () => {
    const params = createGateChainTool.definition.parameters as Record<string, unknown>
    const props = params.properties as Record<string, unknown>
    const required = params.required as string[]
    expect(props).toHaveProperty('ast_json')
    expect(props).toHaveProperty('gate_id')
    expect(props).toHaveProperty('action')
    expect(required).toContain('gate_id')
  })
})

describe('createGateChainTool — execute', () => {
  it('缺少 ast_json 时返回错误', async () => {
    const result = await createGateChainTool.execute({
      gate_id: 'test_gate',
    })
    expect(result.success).toBe(false)
    expect(result.error).toContain('ast_json')
  })

  it('无效 JSON 时返回错误', async () => {
    const result = await createGateChainTool.execute({
      gate_id: 'test_gate',
      ast_json: 'not-json',
    })
    expect(result.success).toBe(false)
    expect(result.error).toContain('JSON')
  })

  it('应编译简单的 condition 节点', async () => {
    const result = await createGateChainTool.execute({
      gate_id: 'amount_gate',
      ast_json: JSON.stringify({
        type: 'condition',
        field: 'amount',
        op: 'gt',
        value: '10000',
      }),
    })
    expect(result.success).toBe(true)
    const data = result.data as Record<string, unknown>
    const compiled = data.compiled as Record<string, unknown>
    const steps = compiled.steps as Array<{ type: string; description: string }>
    expect(steps).toHaveLength(1)
    expect(steps[0].type).toBe('condition')
    expect(steps[0].description).toContain('amount')
  })

  it('应编译 and/or/then 嵌套 AST', async () => {
    const result = await createGateChainTool.execute({
      gate_id: 'approval_gate',
      scenario: 'production',
      ast_json: JSON.stringify({
        type: 'then',
        when: {
          type: 'and',
          left: { type: 'condition', field: 'amount', op: 'gt', value: '10000' },
          right: { type: 'condition', field: 'department', op: 'eq', value: 'finance' },
        },
        do: {
          type: 'action',
          name: 'require_approval',
          value: 'director',
        },
      }),
    })
    expect(result.success).toBe(true)
    const data = result.data as Record<string, unknown>
    expect(data.gate_id).toBe('approval_gate')
    expect(data.scenario).toBe('production')
    const compiled = data.compiled as Record<string, unknown>
    const steps = compiled.steps as Array<{ type: string; description: string }>
    expect(steps.length).toBeGreaterThanOrEqual(4)
    expect(steps.some((s) => s.type === 'do_branch')).toBe(true)
    expect(steps.some((s) => s.type === 'action')).toBe(true)
  })

  it('无效 AST 结构时返回错误', async () => {
    const result = await createGateChainTool.execute({
      gate_id: 'bad_gate',
      ast_json: JSON.stringify({ type: 'unknown_type' }),
    })
    expect(result.success).toBe(false)
    expect(result.error).toContain('未知')
  })

  it('remove_rule 应删除已注册的 custom gate 链', async () => {
    const result = await createGateChainTool.execute({
      gate_id: 'amount_approval',
      action: 'remove_rule',
    })
    expect(result.success).toBe(true)
    const data = result.data as Record<string, unknown>
    expect(data.gate_id).toBe('amount_approval')
    expect(data.deleted).toBe(true)
  })
})
