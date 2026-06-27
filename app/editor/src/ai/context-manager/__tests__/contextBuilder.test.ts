import { describe, it, expect } from 'vitest'
import { buildContext } from '../contextBuilder'
import type { CompactIndex, ToolDelta } from '../contextBuilder'

const SYSTEM_PROMPT = '你是 BlessStar 配置编辑器的 AI 助手。'
const TOOL_DEFS = [
  {
    name: 'create_schema_field',
    description: '创建 Schema 字段',
    parameters: { type: 'object', properties: { key: { type: 'string' } }, required: ['key'] },
  },
  {
    name: 'validate_config',
    description: '校验配置',
    parameters: { type: 'object', properties: { config_json: { type: 'string' } }, required: ['config_json'] },
  },
]

const MOCK_COMPACT: CompactIndex = {
  fieldSemantics: '# field_semantics.compact\nfield_key|type|required|widget|ai_hint\nhost_address|str|true|input|数据库主机地址\n',
  domainKnowledge: '# domain_knowledge.compact\ndomain_name|field_count|field_list\ndatabase|4|host_address,host_port,db_name,db_user\n',
  constraintKnowledge: '# constraint_knowledge.compact\ngate_id|scenario|field_key|op|value\nGATE-001|db_conn|host_address|required|\n',
}

describe('contextBuilder — 基础流程', () => {
  it('基础三层构建：返回 3 条 message（system + user），不超过 4 条', () => {
    const result = buildContext({
      userInput: '创建一个字段',
      systemPrompt: SYSTEM_PROMPT,
      toolDefs: [],
      indexCompact: null,
    })

    expect(result.length).toBe(2) // system + user
    expect(result[0].role).toBe('system')
    expect(result[1].role).toBe('user')
    expect(result[1].content).toBe('创建一个字段')
  })

  it('返回 3-4 条 message，带 tool delta 时不超过 4 条', () => {
    const delta: ToolDelta = { summary: '✅ 已创建字段: host_address (str, required, widget=input)' }

    const result = buildContext({
      userInput: '帮我改这个',
      systemPrompt: SYSTEM_PROMPT,
      toolDefs: [],
      indexCompact: null,
      lastToolDelta: delta,
    })

    expect(result.length).toBe(3) // system + tool_delta(as user) + user
    expect(result[0].role).toBe('system')
    expect(result[1].role).toBe('user')
    expect(result[1].content).toContain(delta.summary)
    expect(result[2].role).toBe('user')
  })

  it('不保留历史：连续调用 2 次，每次返回同样条数', () => {
    const input = {
      userInput: '第一次输入',
      systemPrompt: SYSTEM_PROMPT,
      toolDefs: [],
      indexCompact: null,
    }

    const r1 = buildContext(input)
    const r2 = buildContext({ ...input, userInput: '第二次输入' })

    expect(r1.length).toBe(2)
    expect(r2.length).toBe(2)
    // 第二次不应该包含第一次的 user input
    expect(r2[1].content).not.toContain('第一次输入')
    expect(r2[1].content).toBe('第二次输入')
  })

  it('tool delta 可选：无 lastToolDelta 时不返回 tool role message', () => {
    const result = buildContext({
      userInput: '测试',
      systemPrompt: SYSTEM_PROMPT,
      toolDefs: [],
      indexCompact: null,
      // 没有 lastToolDelta
    })

    expect(result.length).toBe(2)
    expect(result.filter((m) => m.role === 'tool')).toHaveLength(0)
  })

  it('compact index 注入：system message 包含分隔标记', () => {
    const result = buildContext({
      userInput: '查询字段',
      systemPrompt: SYSTEM_PROMPT,
      toolDefs: [],
      indexCompact: MOCK_COMPACT,
    })

    const systemContent = result[0].content
    expect(systemContent).toContain('=== Agent Skill Index ===')
    expect(systemContent).toContain('field_semantics.compact')
    expect(systemContent).toContain('domain_knowledge.compact')
    expect(systemContent).toContain('constraint_knowledge.compact')
  })

  it('system message 含 tool 定义描述', () => {
    const result = buildContext({
      userInput: 'test',
      systemPrompt: SYSTEM_PROMPT,
      toolDefs: TOOL_DEFS,
      indexCompact: null,
    })

    const systemContent = result[0].content
    expect(systemContent).toContain('## 可用工具')
    expect(systemContent).toContain('create_schema_field')
    expect(systemContent).toContain('validate_config')
  })
})
