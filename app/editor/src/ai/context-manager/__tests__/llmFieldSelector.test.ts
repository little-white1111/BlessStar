import { describe, it, expect } from 'vitest'
import { buildFieldSelectionPrompt, parseFieldSelectionResponse, mockFieldSelection } from '../llmFieldSelector'

describe('llmFieldSelector — C 路径 LLM 字段选择器', () => {
  it('buildFieldSelectionPrompt 构造提示词不超过 150 tokens', () => {
    const prompt = buildFieldSelectionPrompt({
      userInput: '我需要一个数据库主机地址字段',
      candidateFields: ['host_address', 'host_port', 'db_name'],
      topK: 2,
    })

    // 粗略估算：~4 chars/token → prompt 应 < 600 char
    expect(prompt.length).toBeLessThan(600)
    expect(prompt).toContain('host_address')
    expect(prompt).toContain('db_name')
    expect(prompt).toContain('2')
  })

  it('buildFieldSelectionPrompt 含用户输入和候选字段', () => {
    const prompt = buildFieldSelectionPrompt({
      userInput: '测试输入',
      candidateFields: ['field_a', 'field_b'],
      topK: 1,
    })

    expect(prompt).toContain('测试输入')
    expect(prompt).toContain('field_a')
  })

  it('parseFieldSelectionResponse 解析首行为字段名', () => {
    const result = parseFieldSelectionResponse('host_address, port\ndatabase 相关')

    expect(result.selectedFields).toEqual(['host_address', 'port'])
    expect(result.reasoning).toContain('database')
  })

  it('parseFieldSelectionResponse 空输入兜底', () => {
    const result = parseFieldSelectionResponse('')
    expect(result.selectedFields).toEqual([])
    expect(result.reasoning).toBeDefined()
  })

  it('mockFieldSelection 返回评分前 K 个', () => {
    const result = mockFieldSelection({
      userInput: 'host',
      candidateFields: ['host_address', 'host_port', 'db_name', 'price'],
      topK: 2,
    })

    expect(result.selectedFields.length).toBe(2)
    expect(result.selectedFields).toContain('host_address')
    expect(result.selectedFields).toContain('host_port')
    expect(result.selectedFields).not.toContain('price')
    expect(result.reasoning).toContain('top=2')
  })

  it('mockFieldSelection 字段名包含输入字符加分', () => {
    const result = mockFieldSelection({
      userInput: 'port',
      candidateFields: ['host_port', 'host_address', 'db_port'],
      topK: 3,
    })

    expect(result.selectedFields).toContain('host_port')
    expect(result.selectedFields).toContain('db_port')
  })
})
