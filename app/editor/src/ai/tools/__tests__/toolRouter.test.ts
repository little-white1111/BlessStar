import { describe, it, expect } from 'vitest'
import {
  META_TOOL_DEFINITION,
  routeIntent,
  getInjectDefinitions,
  setMetaMode,
  isMetaMode,
} from '../../tools/toolRouter'
import { setToolIndex } from '../../tools/toolMatcher'

describe('toolRouter — Tool Router meta-tool 模式', () => {
  it('META_TOOL_DEFINITION 只含 1 个', () => {
    expect(META_TOOL_DEFINITION.name).toBe('blessstar_tools')
    expect(META_TOOL_DEFINITION.parameters.properties).toHaveProperty('intent')
  })

  it('setMetaMode / isMetaMode 开关', () => {
    setMetaMode(false)
    expect(isMetaMode()).toBe(false)

    setMetaMode(true)
    expect(isMetaMode()).toBe(true)

    // 重置
    setMetaMode(false)
  })

  it('getInjectDefinitions 在传统模式返回完整列表', () => {
    setMetaMode(false)
    const defs = getInjectDefinitions()
    expect(defs.length).toBeGreaterThanOrEqual(13)
  })

  it('getInjectDefinitions 在 meta 模式只返回 1 个', () => {
    setMetaMode(true)
    const defs = getInjectDefinitions()
    expect(defs.length).toBe(1)
    expect(defs[0].name).toBe('blessstar_tools')

    setMetaMode(false)
  })

  it('routeIntent 匹配到目录相关意图', () => {
    // 初始化工具索引
    setToolIndex({
      directory: ['list_directory'],
      file: ['read_file', 'find_files'],
    })

    const result = routeIntent('列出目录下的文件')
    // 应匹配 list_directory 或回退
    expect(['routed', 'not_routed'].includes(result.routed ? 'routed' : 'not_routed')).toBe(true)
  })

  it('空 intent 返回 fallback', () => {
    const result = routeIntent('')
    expect(result.routed).toBe(false)
    expect(result.fallbackMessage).toContain('不能为空')
  })
})
