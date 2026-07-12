/**
 * DomainPluginRegistry Unit Tests
 *
 * ADR-全链路接通 不变量 #9 (Plugin 版本契约):
 *   Plugin 注册时进行版本兼容检查，不兼容时拒绝注册。
 * ADR-全链路接通 不变量 #3 (Plugin 隔离性):
 *   各 Plugin 在独立命名空间中注册，互不污染。
 */

import { describe, it, expect, beforeEach } from 'vitest'
import { DomainPluginRegistry } from './registry'
import type { IDomainPlugin } from './types'

/* ── Helpers ─────────────────────────────────────────────────────── */

const createPlugin = (overrides?: Partial<IDomainPlugin>): IDomainPlugin => ({
  id: 'test-plugin',
  version: '1.0.0',
  supportedPipelineVersion: '1.0.0',
  tools: [],
  getSystemPrompt: () => 'test',
  getConceptKnowledge: () => [],
  getTrieDictionary: () => ({ domainKW: {}, opKW: {}, opMap: {} }),
  getCommandRegistry: () => [],
  getPersistenceProvider: () => ({ save: async () => {}, load: async () => null, delete: async () => {}, list: async () => [] }),
  ...overrides,
})

describe('DomainPluginRegistry', () => {
  beforeEach(() => {
    DomainPluginRegistry.clear()
  })

  it('should register a valid plugin', () => {
    const plugin = createPlugin()
    const result = DomainPluginRegistry.register(plugin)

    expect(result).toBe(true)
    expect(DomainPluginRegistry.get('test-plugin')).toBe(plugin)
  })

  it('should reject plugin with unsupported pipeline version', () => {
    const plugin = createPlugin({ supportedPipelineVersion: '0.5.0' })
    const result = DomainPluginRegistry.register(plugin)

    expect(result).toBe(false)
    expect(DomainPluginRegistry.get('test-plugin')).toBeUndefined()
  })

  it('should allow re-registration with same id', () => {
    const p1 = createPlugin({ id: 'dup' })
    const p2 = createPlugin({ id: 'dup', version: '2.0.0' })

    expect(DomainPluginRegistry.register(p1)).toBe(true)
    expect(DomainPluginRegistry.register(p2)).toBe(true) // overwrites
  })

  it('should get all registered plugins', () => {
    DomainPluginRegistry.register(createPlugin({ id: 'a' }))
    DomainPluginRegistry.register(createPlugin({ id: 'b' }))

    const all = DomainPluginRegistry.getAll()
    expect(all).toHaveLength(2)
    expect(all.map(p => p.id).sort()).toEqual(['a', 'b'])
  })

  it('should return default plugin (first registered)', () => {
    DomainPluginRegistry.register(createPlugin({ id: 'first' }))
    DomainPluginRegistry.register(createPlugin({ id: 'second' }))

    const def = DomainPluginRegistry.getDefault()
    expect(def?.id).toBe('first')
  })

  it('should return undefined for unknown plugin id', () => {
    expect(DomainPluginRegistry.get('nonexistent')).toBeUndefined()
  })

  it('should return empty array when no plugins registered', () => {
    DomainPluginRegistry.clear()
    expect(DomainPluginRegistry.getAll()).toEqual([])
    expect(DomainPluginRegistry.getDefault()).toBeUndefined()
  })
})
