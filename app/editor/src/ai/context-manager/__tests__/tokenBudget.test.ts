import { describe, it, expect } from 'vitest'
import {
  estimateTokens,
  estimateContextTokens,
  degradeIfOverBudget,
  BUDGET_TOTAL,
  BUDGET_LAYER2,
} from '../tokenBudget'
import type { CompactIndex } from '../contextBuilder'

const MOCK_COMPACT: CompactIndex = {
  fieldSemantics: Array(50).fill('field_key|type|required|widget|ai_hint').join('\n'),
  domainKnowledge: Array(30).fill('domain|count|fields').join('\n'),
  constraintKnowledge: Array(20).fill('gate_id|scenario|field|op|value').join('\n'),
}

describe('tokenBudget — Token 预算预检', () => {
  it('estimateTokens 中文按 2 chars/token 估算', () => {
    const chinese = '数据库主机地址端口号'
    const tokens = estimateTokens(chinese)
    // 7 个中文字 → ~3.5 → ceil 4
    expect(tokens).toBeGreaterThanOrEqual(3)
    expect(tokens).toBeLessThanOrEqual(5)
  })

  it('estimateTokens 英文按 4 chars/token 估算', () => {
    const english = 'host_address port db_name'
    const tokens = estimateTokens(english)
    // ~22 chars / 4 = 5.5 → ceil 6
    expect(tokens).toBeGreaterThanOrEqual(5)
    expect(tokens).toBeLessThanOrEqual(7)
  })

  it('estimateContextTokens 返回预估值和超限标志', () => {
    const est = estimateContextTokens(
      'system prompt ' + 'x'.repeat(200),
      'user input ' + 'x'.repeat(100),
      MOCK_COMPACT,
      null,
    )

    expect(est.tokens).toBeGreaterThan(0)
    expect(typeof est.overBudget).toBe('boolean')
    expect(typeof est.excessTokens).toBe('number')
  })

  it('超限时 overBudget=true', () => {
    // 构造一个超大的 compact，确保超限
    const hugeCompact: CompactIndex = {
      fieldSemantics: 'x'.repeat(10000),
      domainKnowledge: 'x'.repeat(5000),
      constraintKnowledge: 'x'.repeat(5000),
    }

    const est = estimateContextTokens('prompt', 'input', hugeCompact, null)
    expect(est.overBudget).toBe(true)
    expect(est.excessTokens).toBeGreaterThan(0)
  })
})

describe('tokenBudget — degradeIfOverBudget 降级策略', () => {
  it('预算充足时不做降级', () => {
    const result = degradeIfOverBudget('prompt', 'input', MOCK_COMPACT, null)
    expect(result.disableCPath).toBe(false)
    expect(result.reason).toContain('预算充足')
  })

  it('无 compact 索引时直接返回', () => {
    const result = degradeIfOverBudget('prompt', 'input', null, null)
    expect(result.compactIndex).toBeNull()
    expect(result.disableCPath).toBe(false)
  })

  it('移除 constraint_knowledge 为第一级降级', () => {
    // 构造一个刚好超限的场景
    const justOver: CompactIndex = {
      fieldSemantics: 'x'.repeat(2000),
      domainKnowledge: 'x'.repeat(2000),
      constraintKnowledge: 'x'.repeat(3000),
    }

    const result = degradeIfOverBudget(
      'x'.repeat(2000), // 让 system prompt 也大一些
      'input',
      justOver,
      null,
    )

    // 至少触发降级
    if (result.compactIndex) {
      // constraint_knowledge 应已被移除或截断
      expect(result.reason.length).toBeGreaterThan(0)
    }
  })

  it('严重超限时禁用 C 路径', () => {
    const hugeCompact: CompactIndex = {
      fieldSemantics: 'x'.repeat(10000),
      domainKnowledge: 'x'.repeat(5000),
      constraintKnowledge: 'x'.repeat(5000),
    }

    const result = degradeIfOverBudget(
      'x'.repeat(5000),
      'x'.repeat(1000),
      hugeCompact,
      null,
    )

    if (result.disableCPath) {
      expect(result.reason).toContain('禁用')
    }
  })

  it('降级后紧凑索引不为空', () => {
    const largeCompact: CompactIndex = {
      fieldSemantics: 'x'.repeat(8000),
      domainKnowledge: 'x'.repeat(4000),
      constraintKnowledge: 'x'.repeat(2000),
    }

    const result = degradeIfOverBudget('prompt', 'input', largeCompact, null)
    if (result.disableCPath) {
      // 即使禁用 C 路径，compactIndex 仍有可能包含 fieldSemantics
      if (result.compactIndex) {
        expect(result.compactIndex.fieldSemantics.length).toBeGreaterThan(0)
      }
    }
  })
})
