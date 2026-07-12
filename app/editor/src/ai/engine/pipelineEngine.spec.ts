/**
 * PipelineEngine Unit Tests
 *
 * ADR-全链路接通 — 验证：
 * - Stage 编排顺序正确
 * - TraceId 格式为 `trace-`
 * - Hook 系统通过 domainPlugin.hooks 调用
 * - 错误传播
 * - Context 传递
 */

import { describe, it, expect, vi, beforeEach } from 'vitest'
import { PipelineEngine } from './pipelineEngine'
import type { IPipelineStage, PipelineContext } from './types'

/* ── Mock Stages ─────────────────────────────────────────────────── */

const createMockStage = (id: string): IPipelineStage => ({
  id,
  execute: vi.fn().mockResolvedValue({ status: 'success', stageId: id, durationMs: 1 }),
})

/* ── Mock DomainPlugin ───────────────────────────────────────────── */

const createMockPlugin = () => ({
  id: 'test-plugin',
  version: '1.0.0',
  supportedPipelineVersion: '1.0.0',
  tools: [],
  getSystemPrompt: () => 'test prompt',
  getConceptKnowledge: () => [],
  getTrieDictionary: () => ({ domainKW: {}, opKW: {}, opMap: {} }),
  getCommandRegistry: () => [],
  getPersistenceProvider: () => ({ save: vi.fn(), load: vi.fn(), delete: vi.fn(), list: vi.fn() }),
})

describe('PipelineEngine', () => {
  let engine: PipelineEngine
  let mockPlugin: ReturnType<typeof createMockPlugin>

  beforeEach(() => {
    engine = new PipelineEngine()
    mockPlugin = createMockPlugin()
  })

  it('should execute stages in registration order', async () => {
    const stage1 = createMockStage('stage-1')
    const stage2 = createMockStage('stage-2')
    const stage3 = createMockStage('stage-3')

    engine.addStage(stage1)
    engine.addStage(stage2)
    engine.addStage(stage3)

    const { results } = await engine.execute({
      userMessage: 'test',
      domainPlugin: mockPlugin as any,
    })

    expect(results).toHaveLength(3)
    expect(results[0].stageId).toBe('stage-1')
    expect(results[1].stageId).toBe('stage-2')
    expect(results[2].stageId).toBe('stage-3')
  })

  it('should generate a unique traceId per execution', async () => {
    engine.addStage(createMockStage('s1'))

    const { traceId: id1 } = await engine.execute({
      userMessage: 'first',
      domainPlugin: mockPlugin as any,
    })

    const { traceId: id2 } = await engine.execute({
      userMessage: 'second',
      domainPlugin: mockPlugin as any,
    })

    expect(id1).toMatch(/^trace-/)
    expect(id2).toMatch(/^trace-/)
    expect(id1).not.toBe(id2)
  })

  it('should propagate stage errors', async () => {
    const badStage: IPipelineStage = {
      id: 'fail-stage',
      execute: vi.fn().mockRejectedValue(new Error('stage exploded')),
    }
    engine.addStage(badStage)

    const { results } = await engine.execute({
      userMessage: 'test',
      domainPlugin: mockPlugin as any,
    })

    expect(results[0].status).toBe('error')
    expect(results[0].error).toContain('stage exploded')
  })

  it('should call domainPlugin.hooks.onBeforeStage if available', async () => {
    const beforeHook = vi.fn()
    const pluginWithHooks = {
      ...createMockPlugin(),
      hooks: {
        onBeforeStage: beforeHook,
        onAfterStage: vi.fn(),
      },
    }

    engine.addStage(createMockStage('s1'))

    await engine.execute({
      userMessage: 'test',
      domainPlugin: pluginWithHooks as any,
    })

    expect(beforeHook).toHaveBeenCalledTimes(1)
    expect(beforeHook).toHaveBeenCalledWith('s1', expect.anything())
  })

  it('should call domainPlugin.hooks.onAfterStage if available', async () => {
    const afterHook = vi.fn()
    const pluginWithHooks = {
      ...createMockPlugin(),
      hooks: {
        onBeforeStage: vi.fn(),
        onAfterStage: afterHook,
      },
    }

    engine.addStage(createMockStage('s1'))

    await engine.execute({
      userMessage: 'test',
      domainPlugin: pluginWithHooks as any,
    })

    expect(afterHook).toHaveBeenCalledTimes(1)
    expect(afterHook).toHaveBeenCalledWith('s1', expect.objectContaining({ stageId: 's1', status: 'success' }), expect.anything())
  })

  it('should pass context through stage chain', async () => {
    const stageA: IPipelineStage = {
      id: 'enrich',
      execute: vi.fn().mockImplementation(async (ctx: PipelineContext) => {
        ;(ctx as any).enriched = true
        return { status: 'success', stageId: 'enrich', durationMs: 1 }
      }),
    }
    const stageB: IPipelineStage = {
      id: 'verify',
      execute: vi.fn().mockImplementation(async (ctx: PipelineContext) => {
        if (!(ctx as any).enriched) {
          return { status: 'error', stageId: 'verify', error: 'context not enriched', durationMs: 1 }
        }
        return { status: 'success', stageId: 'verify', durationMs: 1 }
      }),
    }
    engine.addStage(stageA)
    engine.addStage(stageB)

    const { results } = await engine.execute({
      userMessage: 'test',
      domainPlugin: mockPlugin as any,
    })

    expect(results[0].status).toBe('success')
    expect(results[1].status).toBe('success')
  })

  it('should handle empty stage list', async () => {
    const { results } = await engine.execute({
      userMessage: 'test',
      domainPlugin: mockPlugin as any,
    })

    expect(results).toHaveLength(0)
  })

  it('should stop on error and not execute subsequent stages', async () => {
    const goodStage = createMockStage('good')
    const badStage: IPipelineStage = {
      id: 'bad',
      execute: vi.fn().mockRejectedValue(new Error('fail')),
    }
    const neverReached: IPipelineStage = {
      id: 'never',
      execute: vi.fn(),
    }

    engine.addStage(goodStage)
    engine.addStage(badStage)
    engine.addStage(neverReached)

    const { results } = await engine.execute({
      userMessage: 'test',
      domainPlugin: mockPlugin as any,
    })

    expect(results).toHaveLength(2)
    expect(results[0].status).toBe('success')
    expect(results[1].status).toBe('error')
    expect(neverReached.execute).not.toHaveBeenCalled()
  })

  it('should record duration for each stage', async () => {
    engine.addStage(createMockStage('s1'))
    engine.addStage(createMockStage('s2'))

    const { results } = await engine.execute({
      userMessage: 'test',
      domainPlugin: mockPlugin as any,
    })

    for (const r of results) {
      expect(r.durationMs).toBeGreaterThanOrEqual(0)
    }
  })
})
