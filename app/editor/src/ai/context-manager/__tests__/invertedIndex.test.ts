import { describe, it, expect } from 'vitest'
import * as fs from 'fs'
import * as path from 'path'

interface InvertedIndex {
  [keyword: string]: string[]
}

function loadIndex(): InvertedIndex {
  // Relative from repo root (CWD when vitest runs in app/editor/)
  const indexPath = path.resolve('../../tools/indexing/inverted_index.json')
  const raw = fs.readFileSync(indexPath, 'utf-8')
  return JSON.parse(raw)
}

/**
 * 倒排索引查询：不区分大小写匹配
 */
function searchIndex(index: InvertedIndex, query: string): string[] {
  const lowerQuery = query.toLowerCase()
  const results: string[] = []

  for (const [keyword, fields] of Object.entries(index)) {
    if (keyword.toLowerCase().includes(lowerQuery) || lowerQuery.includes(keyword.toLowerCase())) {
      results.push(...fields)
    }
  }

  // deduplicate
  return [...new Set(results)]
}

describe('倒排索引验证', () => {
  let index: InvertedIndex

  beforeEach(() => {
    index = loadIndex()
  })

  it('关键词命中：金额 → amount, total_amount, price', () => {
    const result = searchIndex(index, '金额')
    expect(result).toContain('amount')
    expect(result).toContain('total_amount')
    expect(result).toContain('price')
  })

  it('不区分大小写：Ssl → ssl_enabled', () => {
    const result = searchIndex(index, 'Ssl')
    expect(result).toContain('ssl_enabled')
  })

  it('无匹配返回空数组', () => {
    const result = searchIndex(index, '不存在的关键词')
    expect(result).toEqual([])
  })

  it('部分匹配：连接 → 连接超时、连接池相关字段', () => {
    const result = searchIndex(index, '连接')
    expect(result).toContain('connection_timeout')
    expect(result).toContain('pool_min_size')
  })

  it('索引条目不为空', () => {
    expect(Object.keys(index).length).toBeGreaterThan(0)
  })
})
