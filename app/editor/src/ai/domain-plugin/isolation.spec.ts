/**
 * Plugin Isolation Tests
 *
 * ADR-全链路接通 不变量 #3 (Plugin 隔离性):
 *   两个 DomainPlugin 同时注册时，各 Plugin 的工具集、概念、命令互相隔离。
 */

import { describe, it, expect, beforeEach } from 'vitest'
import { DomainPluginRegistry } from './registry'
import type { IDomainPlugin } from './types'

/* ── Two distinct plugins ────────────────────────────────────────── */

const pluginA: IDomainPlugin = {
  id: 'plugin-a',
  version: '1.0.0',
  supportedPipelineVersion: '1.0.0',
  tools: [
    {
      name: 'a_tool',
      description: 'tool from A',
      parameters: { type: 'object', properties: {}, required: [] },
      execute: async () => ({ toolName: 'a_tool', status: 'success' as const, durationMs: 0 }),
    },
  ],
  getSystemPrompt: () => 'prompt A',
  getConceptKnowledge: () => [{ key: 'concept_a', description: 'from A', keywords: ['a'] }],
  getTrieDictionary: () => ({ domainKW: { a: 'a' }, opKW: {}, opMap: {} }),
  getCommandRegistry: () => [{ command: 'cmd_a', description: 'cmd from A', handler: async () => 'A' }],
  getPersistenceProvider: () => ({ save: async () => {}, load: async () => null, delete: async () => {}, list: async () => [] }),
}

const pluginB: IDomainPlugin = {
  id: 'plugin-b',
  version: '1.0.0',
  supportedPipelineVersion: '1.0.0',
  tools: [
    {
      name: 'b_tool',
      description: 'tool from B',
      parameters: { type: 'object', properties: {}, required: [] },
      execute: async () => ({ toolName: 'b_tool', status: 'success' as const, durationMs: 0 }),
    },
  ],
  getSystemPrompt: () => 'prompt B',
  getConceptKnowledge: () => [{ key: 'concept_b', description: 'from B', keywords: ['b'] }],
  getTrieDictionary: () => ({ domainKW: { b: 'b' }, opKW: {}, opMap: {} }),
  getCommandRegistry: () => [{ command: 'cmd_b', description: 'cmd from B', handler: async () => 'B' }],
  getPersistenceProvider: () => ({ save: async () => {}, load: async () => null, delete: async () => {}, list: async () => [] }),
}

describe('Plugin Isolation (不变量 #3)', () => {
  beforeEach(() => {
    DomainPluginRegistry.clear()
  })

  it('both plugins should be registered independently', () => {
    DomainPluginRegistry.register(pluginA)
    DomainPluginRegistry.register(pluginB)

    expect(DomainPluginRegistry.getAll()).toHaveLength(2)
  })

  it('tools should be isolated per plugin', () => {
    DomainPluginRegistry.register(pluginA)
    DomainPluginRegistry.register(pluginB)

    const a = DomainPluginRegistry.get('plugin-a')!
    const b = DomainPluginRegistry.get('plugin-b')!

    expect(a.tools).toHaveLength(1)
    expect(a.tools[0].name).toBe('a_tool')
    expect(b.tools).toHaveLength(1)
    expect(b.tools[0].name).toBe('b_tool')
  })

  it('concepts should be isolated per plugin', () => {
    DomainPluginRegistry.register(pluginA)
    DomainPluginRegistry.register(pluginB)

    const a = DomainPluginRegistry.get('plugin-a')!
    const b = DomainPluginRegistry.get('plugin-b')!

    expect(a.getConceptKnowledge()[0].key).toBe('concept_a')
    expect(b.getConceptKnowledge()[0].key).toBe('concept_b')
  })

  it('commands should be isolated per plugin', () => {
    DomainPluginRegistry.register(pluginA)
    DomainPluginRegistry.register(pluginB)

    const a = DomainPluginRegistry.get('plugin-a')!
    const b = DomainPluginRegistry.get('plugin-b')!

    expect(a.getCommandRegistry()[0].command).toBe('cmd_a')
    expect(b.getCommandRegistry()[0].command).toBe('cmd_b')
  })

  it('prompts should be isolated per plugin', () => {
    DomainPluginRegistry.register(pluginA)
    DomainPluginRegistry.register(pluginB)

    const a = DomainPluginRegistry.get('plugin-a')!
    const b = DomainPluginRegistry.get('plugin-b')!

    expect(a.getSystemPrompt()).toBe('prompt A')
    expect(b.getSystemPrompt()).toBe('prompt B')
  })
})
