/**
 * Contract Tests — IDomainPlugin 接口合规性检查
 *
 * ADR-全链路接通 — 验证所有 DomainPlugin 实现满足接口契约：
 * - 所有必需字段非空
 * - 版本兼容
 * - 方法返回格式正确
 */

import { describe, it, expect } from 'vitest'
import { DomainPluginRegistry } from './registry'
import { blessstarPlugin } from '../domains/blessstar/index'
import type { IDomainPlugin } from './types'

/**
 * Verify that a plugin satisfies the IDomainPlugin contract.
 */
function verifyPluginContract(plugin: IDomainPlugin): void {
  // ── Identity ──
  expect(plugin.id).toBeDefined()
  expect(typeof plugin.id).toBe('string')
  expect(plugin.id.length).toBeGreaterThan(0)

  expect(plugin.version).toBeDefined()
  expect(typeof plugin.version).toBe('string')
  expect(plugin.version).toMatch(/^\d+\.\d+\.\d+$/)

  expect(plugin.supportedPipelineVersion).toBeDefined()
  expect(typeof plugin.supportedPipelineVersion).toBe('string')
  expect(plugin.supportedPipelineVersion).toMatch(/^\d+\.\d+\.\d+$/)

  // ── Tools ──
  expect(Array.isArray(plugin.tools)).toBe(true)
  for (const tool of plugin.tools) {
    expect(tool.name).toBeDefined()
    expect(typeof tool.name).toBe('string')
    expect(tool.name.length).toBeGreaterThan(0)

    expect(tool.description).toBeDefined()
    expect(typeof tool.description).toBe('string')

    expect(tool.parameters).toBeDefined()
    expect(tool.parameters.type).toBe('object')

    expect(typeof tool.execute).toBe('function')
  }

  // ── Methods ──
  expect(typeof plugin.getSystemPrompt).toBe('function')
  const prompt = plugin.getSystemPrompt()
  expect(typeof prompt).toBe('string')
  expect(prompt.length).toBeGreaterThan(0)

  expect(typeof plugin.getConceptKnowledge).toBe('function')
  const concepts = plugin.getConceptKnowledge()
  expect(Array.isArray(concepts)).toBe(true)
  for (const c of concepts) {
    expect(c.key).toBeDefined()
    expect(c.description).toBeDefined()
    expect(Array.isArray(c.keywords)).toBe(true)
  }

  expect(typeof plugin.getTrieDictionary).toBe('function')
  const trie = plugin.getTrieDictionary()
  expect(trie.domainKW).toBeDefined()
  expect(trie.opKW).toBeDefined()
  expect(trie.opMap).toBeDefined()

  expect(typeof plugin.getCommandRegistry).toBe('function')
  const commands = plugin.getCommandRegistry()
  expect(Array.isArray(commands)).toBe(true)
  for (const cmd of commands) {
    expect(cmd.command).toBeDefined()
    expect(typeof cmd.handler).toBe('function')
  }

  expect(typeof plugin.getPersistenceProvider).toBe('function')
  const provider = plugin.getPersistenceProvider()
  expect(typeof provider.save).toBe('function')
  expect(typeof provider.load).toBe('function')
  expect(typeof provider.delete).toBe('function')
  expect(typeof provider.list).toBe('function')
}

describe('BlessStarDomainPlugin Contract', () => {
  it('should satisfy IDomainPlugin contract', () => {
    verifyPluginContract(blessstarPlugin)
  })

  it('should have exactly 6 domain tools', () => {
    expect(blessstarPlugin.tools).toHaveLength(6)
    const names = blessstarPlugin.tools.map(t => t.name)
    expect(names).toEqual([
      'blessstar_read_config',
      'blessstar_write_config',
      'blessstar_validate_config',
      'blessstar_list_configs',
      'blessstar_workspace_build',
      'blessstar_workspace_export',
    ])
  })

  it('should have 6 commands', () => {
    const commands = blessstarPlugin.getCommandRegistry()
    expect(commands).toHaveLength(6)
    const cmdNames = commands.map(c => c.command)
    expect(cmdNames).toEqual(['read', 'write', 'list', 'validate', 'build', 'help'])
  })

  it('should have 10 concept entries', () => {
    const concepts = blessstarPlugin.getConceptKnowledge()
    expect(concepts).toHaveLength(10)
  })

  it('should have non-empty system prompt', () => {
    const prompt = blessstarPlugin.getSystemPrompt()
    expect(prompt).toContain('BlessStar Config Editor')
    expect(prompt).toContain('server.port')
  })

  it('should have functional Trie dictionary', () => {
    const trie = blessstarPlugin.getTrieDictionary()
    expect(trie.domainKW['config']).toBe('blessstar')
    expect(trie.opKW['读取']).toBe('读')
    expect(trie.opMap['读']).toBe('read')
  })
})

describe('Registered Plugin Contracts', () => {
  it('all registered plugins should satisfy contract', () => {
    // Clear and register the plugin first
    DomainPluginRegistry.clear()
    const registered = DomainPluginRegistry.register(blessstarPlugin)
    expect(registered).toBe(true)

    const plugins = DomainPluginRegistry.getAll()
    expect(plugins.length).toBeGreaterThan(0)

    for (const plugin of plugins) {
      verifyPluginContract(plugin)
    }
  })
})
