/**
 * parallelExecutor 测试
 */
import { describe, it, expect } from 'vitest'
import { splitUserIntent } from '../parallelExecutor'

describe('parallelExecutor — splitUserIntent', () => {
  it('空输入应返回空数组', () => {
    expect(splitUserIntent('')).toEqual([])
    expect(splitUserIntent('  ')).toEqual([])
  })

  it('单句不切', () => {
    const result = splitUserIntent('把房间号改成10041')
    expect(result).toEqual(['把房间号改成10041'])
  })

  it('句号切分多句', () => {
    const result = splitUserIntent('把房间号改成10041。禁用10级以下弹幕。')
    expect(result).toHaveLength(2)
    expect(result[0]).toBe('把房间号改成10041')
    expect(result[1]).toBe('禁用10级以下弹幕')
  })

  it('分号切分多句', () => {
    const result = splitUserIntent('改A；改B；改C')
    expect(result).toHaveLength(3)
  })

  it('守卫规则：数字内逗号不切', () => {
    // "金额大于10,000元" 应为单句
    const result = splitUserIntent('金额大于10,000元')
    expect(result).toHaveLength(1)
    expect(result[0]).toBe('金额大于10,000元')
  })

  it('多种分隔符混合', () => {
    const result = splitUserIntent('改A。改B；改C\n改D')
    expect(result).toHaveLength(4)
  })

  it('换行切分', () => {
    const result = splitUserIntent('第一句\n第二句')
    expect(result).toHaveLength(2)
    expect(result[0]).toBe('第一句')
    expect(result[1]).toBe('第二句')
  })
})
