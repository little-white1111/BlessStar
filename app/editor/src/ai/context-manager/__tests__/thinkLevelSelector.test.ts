import { describe, it, expect } from 'vitest'
import { selectThinkLevel, getThinkLevelConfig } from '../thinkLevelSelector'

describe('thinkLevelSelector — 推理强度自动选择', () => {
  const context = {
    userInput: '',
    skillRouterEnabled: true,
    metaModeEnabled: true,
  }

  it('/command 输入 → non_think', () => {
    const config = selectThinkLevel('/createconfig 添加房间号', context)
    expect(config.level).toBe('non_think')
    expect(config.requiresLLM).toBe(false)
    expect(config.suggestedTokenBudget).toBe(0)
  })

  it('含文件路径的 dir 请求 → 至少返回合法级别', () => {
    const config = selectThinkLevel('看看 C:\\models 目录下有哪些文件', context)
    expect(['non_think', 'think_low', 'think_high']).toContain(config.level)
  })

  it('一般自然语言请求 → think_high', () => {
    const config = selectThinkLevel('帮我配置一下这个系统的参数，需要修改数据库连接', context)
    expect(config.level).toBe('think_high')
    expect(config.requiresLLM).toBe(true)
    expect(config.suggestedTemperature).toBe(0.3)
  })

  it('空输入 → think_high', () => {
    const config = selectThinkLevel('', context)
    expect(config.level).toBe('think_high')
  })

  it('getThinkLevelConfig 返回配置', () => {
    const config = getThinkLevelConfig('non_think')
    expect(config.suggestedTokenBudget).toBe(0)
    expect(config.requiresLLM).toBe(false)
  })
})
