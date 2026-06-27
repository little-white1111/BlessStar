import { describe, it, expect } from 'vitest'
import { buildToolDelta } from '../toolDeltaFormatter'

describe('toolDeltaFormatter — 压缩摘要', () => {
  it('create_schema_field 压缩：含 field_key 和 type', () => {
    const result = buildToolDelta('create_schema_field', {
      success: true,
      data: { field: { key: 'host_address', type: 'str', required: true, widget: 'input' } },
    })

    expect(result.summary.length).toBeLessThan(200)
    expect(result.summary).toContain('host_address')
    expect(result.summary).toContain('str')
    expect(result.summary).toContain('required')
    expect(result.summary).toContain('widget=input')
  })

  it('validate_config 成功：摘要格式正确', () => {
    const result = buildToolDelta('validate_config', {
      success: true,
      data: { valid: true, message: '配置校验通过 ✓' },
    })

    expect(result.summary).toContain('✅')
    expect(result.summary).toContain('0 个错误')
  })

  it('validate_config 失败：含 "❌" 和第一个错误', () => {
    const result = buildToolDelta('validate_config', {
      success: false,
      error: 'config_json 格式无效',
    })

    expect(result.summary).toContain('❌')
    expect(result.summary).toContain('config_json')
  })

  it('chat 压缩：截取前50字符', () => {
    const result = buildToolDelta('chat', {
      success: true,
      data: { reply: '你好，我来帮你了解 BlessStar 配置系统的基本概念和使用方法' },
    })

    expect(result.summary).toContain('你好')
  })

  it('空结果兜底：返回 "✅ 操作成功" 或 "❌"', () => {
    const successResult = buildToolDelta('unknown_tool', { success: true })
    expect(successResult.summary).toBe('✅ 操作成功')

    const failResult = buildToolDelta('unknown_tool', { success: false, error: '出错了' })
    expect(failResult.summary).toContain('❌')
  })

  it('update_gate_rule 压缩：含 gate_id', () => {
    const result = buildToolDelta('update_gate_rule', {
      success: true,
      data: { gate_id: 'GATE-001', gateJson: '{"gate_id":"GATE-001"}' },
    })

    expect(result.summary).toContain('GATE-001')
  })

  it('generate_normalizer_template 压缩：含厂商名称和映射数', () => {
    const result = buildToolDelta('generate_normalizer_template', {
      success: true,
      data: {
        template: { source_vendor: 'yonyou', mapping: [{}, {}, {}] },
      },
    })

    expect(result.summary).toContain('yonyou')
    expect(result.summary).toContain('3')
  })

  it('null/undefined 结果返回错误', () => {
    const r1 = buildToolDelta('create_schema_field', null)
    expect(r1.summary).toContain('❌')

    const r2 = buildToolDelta('create_schema_field', undefined)
    expect(r2.summary).toContain('❌')
  })

  it('摘要 < 200 字符（接近 token 约束）', () => {
    const result = buildToolDelta('create_schema_field', {
      success: true,
      data: { field: { key: 'a_very_long_field_name_that_should_not_break', type: 'str', required: true, widget: 'input' } },
    })

    expect(result.summary.length).toBeLessThan(200)
  })
})
