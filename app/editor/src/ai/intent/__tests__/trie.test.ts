/**
 * intent trie 测试：trie_dict.ts + trie_matcher.ts
 *
 * 注意：DOMAIN_KW 出厂仅含通用领域词，业务领域词由 BusinessAdapterRegistry 注入。
 * 测试不依赖业务数据，仅验证通用功能。
 */
import { describe, it, expect } from 'vitest'
import { OP_KW, DOMAIN_KW, OP_MAP, findLongestKeyword, extractRuleFragment } from '../trie_dict'
import { compressIntent } from '../trie_matcher'

describe('trie_dict — OP_KW 操作关键词表', () => {
  it('应包含 write 类操作词', () => {
    expect(OP_KW).toHaveProperty('改成', 'write')
    expect(OP_KW).toHaveProperty('设为', 'write')
    expect(OP_KW).toHaveProperty('写入', 'write')
  })

  it('应包含 gate 类操作词', () => {
    expect(OP_KW).toHaveProperty('禁用', 'gate')
    expect(OP_KW).toHaveProperty('启用', 'gate')
    expect(OP_KW).toHaveProperty('限制', 'gate')
  })

  it('应包含 schema 类操作词', () => {
    expect(OP_KW).toHaveProperty('删除', 'schema')
    expect(OP_KW).toHaveProperty('新增', 'schema')
  })

  it('应包含 read/list/search 类操作词', () => {
    expect(OP_KW).toHaveProperty('查看', 'read')
    expect(OP_KW).toHaveProperty('显示', 'list')
    expect(OP_KW).toHaveProperty('搜索', 'search')
  })
})

describe('trie_dict — DOMAIN_KW 领域关键词表（出厂基线）', () => {
  it('应仅含通用 schema.field 领域词', () => {
    expect(DOMAIN_KW).toHaveProperty('字段', 'schema.field')
    // 业务领域词（如 livedesign、fin）由 adapter 注入
    expect(Object.keys(DOMAIN_KW).length).toBe(1)
  })
})

describe('trie_dict — OP_MAP 操作符映射', () => {
  it('应包含基本比较操作符', () => {
    expect(OP_MAP).toHaveProperty('大于', 'gt')
    expect(OP_MAP).toHaveProperty('小于', 'lt')
    expect(OP_MAP).toHaveProperty('等于', 'eq')
    expect(OP_MAP).toHaveProperty('大于等于', 'gte')
  })
})

describe('trie_dict — findLongestKeyword', () => {
  it('应匹配通用 schema.field 关键词', () => {
    const result = findLongestKeyword('添加一个字段', DOMAIN_KW)
    expect(result).not.toBeNull()
    expect(result!.value).toBe('schema.field')
  })

  it('无匹配时返回 null', () => {
    const result = findLongestKeyword('今天天气不错', DOMAIN_KW)
    expect(result).toBeNull()
  })
})

describe('trie_dict — extractRuleFragment', () => {
  it('应从 "大于" 提取 gt 操作符', () => {
    const result = extractRuleFragment('金额大于10000')
    expect(result).toEqual({ op: 'gt', value: '10000' })
  })

  it('应从 "小于" 提取 lt 操作符', () => {
    const result = extractRuleFragment('小于50')
    expect(result).toEqual({ op: 'lt', value: '50' })
  })

  it('应从 "以上" 提取 gte', () => {
    const result = extractRuleFragment('10级以上')
    expect(result).toEqual({ op: 'gte', value: '10' })
  })

  it('纯数字句子仍返回 eq rule（供 compressIntent 消费）', () => {
    const result = extractRuleFragment('把房间号改成10041')
    expect(result).toEqual({ op: 'eq', value: '10041' })
  })

  it('不包含数字时返回 undefined', () => {
    const result = extractRuleFragment('把房间号改成新值')
    expect(result).toBeUndefined()
  })
})

describe('trie_matcher — compressIntent（出厂基线）', () => {
  it('通用操作词+通用领域词应压缩为一个 intent', () => {
    // '字段' 是核心 DOMAIN_KW，'添加' 是 schema 操作
    const result = compressIntent('添加字段')
    expect(result).not.toBeNull()
    expect(result!.operation).toBe('schema')
    expect(result!.config.domain).toBe('schema.field')
  })

  it('无操作词时应返回 null（降级给 LLM）', () => {
    const result = compressIntent('今天天气怎么样')
    expect(result).toBeNull()
  })

  it('无领域词时应返回 null（降级给 LLM）', () => {
    const result = compressIntent('帮我查一下数据')
    expect(result).toBeNull()
  })
})
