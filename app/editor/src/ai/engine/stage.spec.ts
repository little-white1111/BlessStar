/**
 * Stage Unit Tests — built-in Stage implementations
 *
 * ADR-全链路接通 — 验证 StageRouter / StageIntent / StageExecute / StageRender
 * 的独立逻辑。
 */

import { describe, it, expect, vi } from 'vitest'
import { StageRouter, StageIntent, StageExecute, StageRender } from './stage'
import type { PipelineContext } from './types'

/* ── Helper ──────────────────────────────────────────────────────── */

const createContext = (overrides?: Partial<PipelineContext>): PipelineContext => ({
  traceId: 'test-trace',
  userMessage: 'test message',
  clauses: [],
  currentIntent: null,
  toolResults: [],
  metadata: {},
  domainPlugin: {
    id: 'test-plugin',
    version: '1.0.0',
    supportedPipelineVersion: '1.0.0',
    tools: [],
    getSystemPrompt: () => 'prompt',
    getConceptKnowledge: () => [],
    getTrieDictionary: () => ({ domainKW: {}, opKW: {}, opMap: {} }),
    getCommandRegistry: () => [],
    getPersistenceProvider: () => ({ save: vi.fn(), load: vi.fn(), delete: vi.fn(), list: vi.fn() }),
  },
  abortSignal: new AbortController().signal,
  ...overrides,
})

/* ── StageRouter ─────────────────────────────────────────────────── */

describe('StageRouter', () => {
  const stage = new StageRouter()

  it('should return success with clause count', async () => {
    const ctx = createContext()
    const result = await stage.execute(ctx)

    expect(result.status).toBe('success')
    expect(result.stageId).toBe('router')
    expect(result.output).toEqual({ clauseCount: 1 })
  })

  it('should split multi-sentence messages into clauses', async () => {
    const ctx = createContext({ userMessage: '修改 server port 为 8080，然后重启服务' })
    const result = await stage.execute(ctx)

    expect(result.status).toBe('success')
    const output = result.output as any
    expect(output.clauseCount).toBe(2)
  })
})

/* ── StageIntent ─────────────────────────────────────────────────── */

describe('StageIntent', () => {
  const stage = new StageIntent()

  it('should return success with intent output', async () => {
    const ctx = createContext()
    const result = await stage.execute(ctx)

    expect(result.status).toBe('success')
    expect(result.stageId).toBe('intent')
    expect(result.output).toBeDefined()
  })

  it('should detect write intent from user message', async () => {
    const ctx = createContext({ userMessage: '把 server.port 改成 8080' })
    const result = await stage.execute(ctx)
    const output = result.output as any

    expect(output.intent.action).toBe('write')
  })

  it('should detect read intent by default', async () => {
    const ctx = createContext({ userMessage: '查看当前配置' })
    const result = await stage.execute(ctx)
    const output = result.output as any

    expect(output.intent.action).toBe('read')
  })
})

/* ── StageExecute ────────────────────────────────────────────────── */

describe('StageExecute', () => {
  const stage = new StageExecute()

  it('should skip execution without intent', async () => {
    const ctx = createContext({ currentIntent: null })
    const result = await stage.execute(ctx)

    expect(result.status).toBe('skipped')
    expect(result.stageId).toBe('execute')
  })

  it('should execute when intent is provided', async () => {
    const ctx = createContext({
      currentIntent: { action: 'read', subject: 'port', target: '', confidence: 0.8 },
    })
    const result = await stage.execute(ctx)

    expect(result.status).toBe('success')
    expect(result.stageId).toBe('execute')
  })

  it('should find matching tool from plugin tools', async () => {
    const ctx = createContext({
      currentIntent: { action: 'write', subject: 'port', target: '', confidence: 0.9 },
      domainPlugin: {
        id: 'test',
        version: '1.0.0',
        supportedPipelineVersion: '1.0.0',
        tools: [
          {
            name: 'blessstar_write_config',
            description: 'write config',
            parameters: { type: 'object', properties: {}, required: [] },
            execute: vi.fn(),
          },
        ],
        getSystemPrompt: () => '',
        getConceptKnowledge: () => [],
        getTrieDictionary: () => ({ domainKW: {}, opKW: {}, opMap: {} }),
        getCommandRegistry: () => [],
        getPersistenceProvider: () => ({ save: vi.fn(), load: vi.fn(), delete: vi.fn(), list: vi.fn() }),
      },
    })
    const result = await stage.execute(ctx)

    expect(result.status).toBe('success')
    const output = result.output as any
    expect(output.matchedTool).toBe('blessstar_write_config')
  })
})

/* ── StageRender ─────────────────────────────────────────────────── */

describe('StageRender', () => {
  const stage = new StageRender()

  it('should return success with summary even with no tool results', async () => {
    const ctx = createContext()
    const result = await stage.execute(ctx)

    expect(result.status).toBe('success')
    expect(result.stageId).toBe('render')
    expect(result.output).toBeDefined()

    const output = result.output as any
    expect(output.summary).toContain('0 tool(s)')
  })
})
