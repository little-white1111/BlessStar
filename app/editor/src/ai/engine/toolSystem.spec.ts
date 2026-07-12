/**
 * ToolSystem Unit Tests
 *
 * ADR-全链路接通 — 验证工具注册/路由/执行以及 OpenAI 兼容的 tool definitions 生成。
 */

import { describe, it, expect, vi, beforeEach } from 'vitest'
import { ToolSystemImpl } from './toolSystem'
import type { IToolDeclaration, PipelineContext } from './types'

describe('ToolSystemImpl', () => {
  let toolSystem: ToolSystemImpl
  let mockTool: IToolDeclaration
  let mockCtx: PipelineContext

  beforeEach(() => {
    toolSystem = new ToolSystemImpl()

    mockTool = {
      name: 'test_tool',
      description: 'A test tool',
      parameters: {
        type: 'object',
        properties: {
          key: { type: 'string' },
          value: { type: 'string' },
        },
        required: ['key'],
      },
      execute: vi.fn().mockResolvedValue({
        toolName: 'test_tool',
        status: 'success',
        data: { result: 'ok' },
        durationMs: 5,
      }),
    }

    mockCtx = {
      traceId: 'test-trace',
      userMessage: 'test',
      clauses: [],
      currentIntent: null,
      toolResults: [],
      metadata: {},
      domainPlugin: {
        id: 'test-plugin',
        version: '1.0.0',
        supportedPipelineVersion: '1.0.0',
        tools: [mockTool],
        getSystemPrompt: () => '',
        getConceptKnowledge: () => [],
        getTrieDictionary: () => ({ domainKW: {}, opKW: {}, opMap: {} }),
        getCommandRegistry: () => [],
        getPersistenceProvider: () => ({ save: vi.fn(), load: vi.fn(), delete: vi.fn(), list: vi.fn() }),
      },
      abortSignal: new AbortController().signal,
    }
  })

  it('should register a tool', () => {
    toolSystem.registerTool(mockTool)
    const tools = toolSystem.getTools()
    expect(tools).toHaveLength(1)
    expect(tools[0].name).toBe('test_tool')
  })

  it('should register multiple tools', () => {
    const tool2: IToolDeclaration = {
      name: 'tool_2',
      description: 'second tool',
      parameters: { type: 'object', properties: {}, required: [] },
      execute: vi.fn(),
    }
    toolSystem.registerTools([mockTool, tool2])

    const tools = toolSystem.getTools()
    expect(tools).toHaveLength(2)
    expect(tools.map(t => t.name).sort()).toEqual(['test_tool', 'tool_2'])
  })

  it('should execute a registered tool by name', async () => {
    toolSystem.registerTool(mockTool)

    const result = await toolSystem.executeTool('test_tool', { key: 'foo' }, mockCtx)

    expect(result.toolName).toBe('test_tool')
    expect(result.status).toBe('success')
    expect(mockTool.execute).toHaveBeenCalledWith(
      { key: 'foo' },
      mockCtx,
    )
  })

  it('should return error for unknown tool', async () => {
    const result = await toolSystem.executeTool('nonexistent', {}, mockCtx)

    expect(result.status).toBe('error')
    expect(result.error).toContain('not found')
  })

  it('should generate OpenAI-compatible tool definitions', () => {
    toolSystem.registerTool(mockTool)
    const tool2: IToolDeclaration = {
      name: 'tool_2',
      description: 'second tool',
      parameters: { type: 'object', properties: {}, required: [] },
      execute: vi.fn(),
    }
    toolSystem.registerTool(tool2)

    const defs = toolSystem.getToolDefinitions()

    expect(defs).toHaveLength(2)
    expect(defs[0]).toEqual({
      type: 'function',
      function: {
        name: 'test_tool',
        description: 'A test tool',
        parameters: {
          type: 'object',
          properties: { key: { type: 'string' }, value: { type: 'string' } },
          required: ['key'],
        },
      },
    })
  })

  it('should handle duplicate registration without throwing', () => {
    toolSystem.registerTool(mockTool)
    expect(() => toolSystem.registerTool(mockTool)).not.toThrow()
    expect(toolSystem.getTools()).toHaveLength(1)
  })

  it('should clear all tools', () => {
    toolSystem.registerTool(mockTool)
    toolSystem.clear()
    expect(toolSystem.getTools()).toHaveLength(0)
  })
})
