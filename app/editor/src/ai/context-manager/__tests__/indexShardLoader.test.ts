import { describe, it, expect } from 'vitest'
import { selectIndexShards, registerDomain, shardResultToCompactIndex } from '../indexShardLoader'

describe('indexShardLoader — Compact Index 分片加载', () => {
  it('连接相关 intent → 匹配 connection domain', () => {
    const result = selectIndexShards('配置数据库连接')
    expect(result.matchedCount).toBeGreaterThanOrEqual(1)
    expect(result.domainKnowledge).toContain('连接')
    expect(result.compressionRatio).toBeGreaterThan(0)
  })

  it('安全相关 intent → 匹配 security domain', () => {
    const result = selectIndexShards('开启 SSL 安全认证')
    expect(result.matchedCount).toBeGreaterThanOrEqual(1)
    expect(result.domainKnowledge).toContain('安全')
  })

  it('不匹配任意 domain → 空结果', () => {
    const result = selectIndexShards('今天天气怎么样')
    expect(result.matchedCount).toBe(0)
    expect(result.domainKnowledge).toBe('')
    expect(result.compressionRatio).toBe(1.0)
  })

  it('空输入 → 空结果', () => {
    const result = selectIndexShards('')
    expect(result.matchedCount).toBe(0)
  })

  it('shardResultToCompactIndex 返回 null 当无匹配', () => {
    const result = selectIndexShards('不相关的内容')
    const compact = shardResultToCompactIndex(result)
    expect(compact).toBeNull()
  })

  it('registerDomain 可注册自定义 domain', () => {
    registerDomain({
      domainName: 'test_domain',
      keywords: ['测试'],
      domainDescription: '测试领域',
      fieldSemantics: 'test: field1/field2',
    })

    const result = selectIndexShards('这是一个测试请求')
    expect(result.matchedCount).toBeGreaterThanOrEqual(1)
    expect(result.domainKnowledge).toContain('测试领域')
  })
})
