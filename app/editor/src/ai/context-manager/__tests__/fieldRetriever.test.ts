import { describe, it, expect } from 'vitest'
import { retrieveFields, scoredRetrieve } from '../fieldRetriever'
import type { InvertedIndex } from '../fieldRetriever'

const MOCK_INDEX: InvertedIndex = {
  '金额': ['amount', 'total_amount', 'price'],
  '主机': ['host_address', 'host_port'],
  '数据库': ['db_host', 'db_port', 'db_name', 'db_user', 'db_password'],
  '超时': ['connection_timeout', 'request_timeout'],
  'SSL': ['ssl_enabled'],
  '连接池': ['pool_min_size', 'pool_max_size', 'pool_timeout'],
}

describe('fieldRetriever — B 路径倒排查找', () => {
  it('精确匹配：金额 → amount, total_amount, price', () => {
    const result = retrieveFields(MOCK_INDEX, '金额')
    expect(result).toContain('amount')
    expect(result).toContain('total_amount')
    expect(result).toContain('price')
  })

  it('包含匹配：连 → pool_min_size, pool_max_size', () => {
    const result = retrieveFields(MOCK_INDEX, '连')
    expect(result).toContain('pool_min_size')
    expect(result).toContain('pool_max_size')
    expect(result).not.toContain('host_address')
  })

  it('无匹配返回空数组', () => {
    const result = retrieveFields(MOCK_INDEX, '不存在的关键词')
    expect(result).toEqual([])
  })

  it('空索引返回空数组', () => {
    const result = retrieveFields({}, '金额')
    expect(result).toEqual([])
  })

  it('空输入返回空数组', () => {
    const result = retrieveFields(MOCK_INDEX, '')
    expect(result).toEqual([])
  })

  it('maxResults 截断', () => {
    // 数据库匹配 5 个字段，但限制 maxResults=2
    const result = retrieveFields(MOCK_INDEX, '数据库', 2)
    expect(result.length).toBeLessThanOrEqual(2)
  })

  it('不区分大小写：ssl → ssl_enabled', () => {
    const result = retrieveFields(MOCK_INDEX, 'ssl')
    expect(result).toContain('ssl_enabled')
  })

  it('结果去重：同一字段不重复出现', () => {
    const index: InvertedIndex = {
      'A': ['foo'],
      'B': ['foo'],
    }
    const result = retrieveFields(index, 'A')
    expect(result).toEqual(['foo'])
  })
})

describe('fieldRetriever — scoredRetrieve 评分排序', () => {
  it('精确匹配得分最高（100）', () => {
    const result = scoredRetrieve(MOCK_INDEX, 'SSL')
    expect(result.length).toBeGreaterThan(0)
    const ssl = result.find((r) => r.field === 'ssl_enabled')
    expect(ssl).toBeDefined()
    expect(ssl!.score).toBe(100)
  })

  it('按分数降序排列', () => {
    const result = scoredRetrieve(MOCK_INDEX, '超时')
    for (let i = 1; i < result.length; i++) {
      expect(result[i].score).toBeLessThanOrEqual(result[i - 1].score)
    }
  })
})
