import { describe, it, expect, beforeEach } from 'vitest'
import { FeedbackCollector } from '../feedbackCollector'

describe('FeedbackCollector — T2 反馈层', () => {
  let collector: FeedbackCollector

  beforeEach(() => {
    collector = new FeedbackCollector()
    collector.clear()
  })

  it('初始状态：无记录', () => {
    expect(collector.getAll()).toHaveLength(0)
  })

  it('record 记录一条反馈', () => {
    const rec = collector.record({
      intent: '查看 models 目录',
      originalOutput: '正在查看...',
      userCorrection: '应该显示文件数量',
      type: 'missing_info',
    })
    expect(rec.id).toContain('fb_')
    expect(rec.timestamp).toBeGreaterThan(0)
    expect(collector.getAll()).toHaveLength(1)
  })

  it('record 记录可选字段（toolName/rating）', () => {
    const rec = collector.record({
      intent: '设置金额',
      originalOutput: '已设置 50000',
      userCorrection: '应该有上限校验',
      type: 'gate_missed',
      toolName: 'write_config_value',
      rating: 3,
    })
    expect(rec.toolName).toBe('write_config_value')
    expect(rec.rating).toBe(3)
  })

  it('getStats 返回统计信息', () => {
    collector.record({ intent: '读文件', originalOutput: 'A', userCorrection: 'B', type: 'translation_incorrect' })
    collector.record({ intent: '读文件', originalOutput: 'A', userCorrection: 'B', type: 'translation_incorrect' })
    collector.record({ intent: '写配置', originalOutput: 'A', userCorrection: 'B', type: 'format_wrong' })

    const stats = collector.getStats()
    expect(stats.total).toBe(3)
    expect(stats.byType.translation_incorrect).toBe(2)
    expect(stats.byType.format_wrong).toBe(1)
    expect(stats.topIntents.length).toBeGreaterThanOrEqual(2)
  })

  it('clear 清空所有记录', () => {
    collector.record({ intent: 'test', originalOutput: 'A', userCorrection: 'B', type: 'other' })
    expect(collector.getAll()).toHaveLength(1)
    collector.clear()
    expect(collector.getAll()).toHaveLength(0)
  })

  it('exportJSONL 导出行格式', () => {
    collector.record({ intent: 'test', originalOutput: 'A', userCorrection: 'B', type: 'other' })
    collector.record({ intent: 'test2', originalOutput: 'C', userCorrection: 'D', type: 'format_wrong' })

    const jsonl = collector.exportJSONL()
    const lines = jsonl.trim().split('\n')
    expect(lines).toHaveLength(2)
    for (const line of lines) {
      expect(() => JSON.parse(line)).not.toThrow()
    }
  })

  it('importJSONL 导入记录', () => {
    const jsonl = [
      JSON.stringify({ id: 'fb_001', timestamp: 1000, intent: 'a', originalOutput: 'o', userCorrection: 'c', type: 'other' }),
      JSON.stringify({ id: 'fb_002', timestamp: 2000, intent: 'b', originalOutput: 'o', userCorrection: 'c', type: 'format_wrong' }),
    ].join('\n')

    const count = collector.importJSONL(jsonl)
    expect(count).toBe(2)
    expect(collector.getAll()).toHaveLength(2)
  })

  it('importJSONL 跳过格式错误的行', () => {
    const jsonl = '{"id": "valid", "intent": "test", "originalOutput": "o", "userCorrection": "c", "type": "other"}\nnot json\n{"also": "invalid"}'
    const count = collector.importJSONL(jsonl)
    expect(count).toBe(1)
    expect(collector.getAll()).toHaveLength(1)
  })
})
