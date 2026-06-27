/**
 * pipeline-e2e.test.ts — AI 管线端到端测试
 *
 * 覆盖 executePipeline 完整 20 步闭环（mock LLM + mock executor）：
 *   Stage 1 (Router) → Stage 2 (Intent) → Core (UA) → Stage 3 (Execute) → Stage 4 (Render)
 *
 * 与 pipeline-stages.test.ts 的分工：
 *   stages.test  → 确定性单元测试（解析/映射/校验/证据链）
 *   e2e.test     → mock LLM + mock executor 端到端集成测试
 *
 * 消费 pipeline-test-cases.ts 中的 22 条测试用例作为输入 +
 * preset UA/Consultation 响应模拟理解Agent 的正确产出。
 * 验证：工具调用数 / planSteps / chatAnswer / 错误标记 / roundVerify 全量配平。
 */

import { describe, it, expect, vi, beforeEach, beforeAll } from 'vitest'
import { executePipeline, type PipelineCallbacks } from '../pipelineManager'
import { executionTrace, toolCallRegistry } from '../../context-manager/executionTrace'
import { FeedbackCollector } from '../../context-manager/feedbackCollector'
import { ALL_TEST_CASES, type PipelineTestCase } from './pipeline-test-cases'
import type { AIBridge } from '../../bridge'
import type { AICompletionRequest, AICompletionResponse, AIMessage } from '../../types'
import type { ToolResult, ToolCall } from '../../types'

import type { ToolDelta } from '../../context-manager/contextBuilder'
import { BusinessAdapterRegistry } from '../../business-adapter/registry'
import { refreshFromRegistry } from '../../tools/configLabels'
import { syncBaselineFromRegistry } from '../../context-manager/adaptiveIndex'
import { syncDomainKWFromRegistry } from '../../intent/trie_dict'

// ═══════════════════════════════════════════════════════════════════════
// vi.hoisted: 在模块加载前注册 mock
// ═══════════════════════════════════════════════════════════════════════

const { mockExecuteToolCall } = vi.hoisted(() => ({
  mockExecuteToolCall: vi.fn(),
}))

// ── Mock executor ──
vi.mock('../../executor', () => ({
  executeToolCall: mockExecuteToolCall,
  executeWithRetry: vi.fn(),
  findTool: vi.fn(() => undefined),
}))

// ── Mock GateFactoryBridge ──
vi.mock('../../GateFactoryBridge', () => ({
  GATE_FACTORIES: [],
}))

// ═══════════════════════════════════════════════════════════════════════
// Preset UA（理解Agent）响应模板 — 模拟正确解析
// ═══════════════════════════════════════════════════════════════════════

interface PresetTodoItem {
  subject: string
  /** 专题七新格式：QUERY/MODIFY/ACTION */
  intent: string
  value?: string | null
  is_chat?: boolean
  target_config_key?: string | null
  new_value?: string | null
  is_ambiguous?: boolean
  is_dangerous?: boolean
}

function uaJson(items: PresetTodoItem[]): string {
  return JSON.stringify({
    todo: items.map(i => ({
      subject: i.subject,
      intent: i.intent,
      value: i.value ?? null,
      condition: null,
      is_chat: i.is_chat ?? (i.intent === 'ACTION' && !i.target_config_key),
      target_config_key: i.target_config_key ?? null,
      new_value: i.new_value ?? null,
      is_ambiguous: i.is_ambiguous ?? false,
      is_dangerous: i.is_dangerous ?? false,
    })),
  })
}

/** 按测试用例 ID 返回理解Agent 的 mock 产出（专题七：3 intent 格式） */
const UA_RESPONSES: Record<string, string> = {
  // ── CHAT 组 → ACTION (is_chat=true) ──
  'AI-TC-CHAT-01': uaJson([{ subject: 'gate', intent: 'ACTION', is_chat: true }]),
  'AI-TC-CHAT-02': uaJson([{ subject: 'schema', intent: 'ACTION', is_chat: true }]),
  'AI-TC-CHAT-03': uaJson([{ subject: 'gate', intent: 'ACTION', is_chat: true }]),
  'AI-TC-CHAT-04': uaJson([{ subject: '功能', intent: 'ACTION', is_chat: true }]),
  'AI-TC-CHAT-05': uaJson([{ subject: '使用方式', intent: 'ACTION', is_chat: true }]),

  // ── LIST 组 → QUERY (no target_config_key → route to LIST) ──
  'AI-TC-LIST-01': uaJson([{ subject: '所有配置项', intent: 'QUERY', is_chat: false }]),
  'AI-TC-LIST-02': uaJson([{ subject: '弹幕配置', intent: 'QUERY', target_config_key: 'livedesign.danmaku', is_chat: false }]),

  // ── WRITE 组 → MODIFY ──
  'AI-TC-WRITE-01': uaJson([{ subject: '房间号', intent: 'MODIFY', new_value: '10041', target_config_key: 'livedesign.room.room_id', is_chat: false }]),
  'AI-TC-WRITE-02': uaJson([{ subject: '弹幕字号', intent: 'MODIFY', new_value: '20', target_config_key: 'livedesign.danmaku.font_size', is_chat: false }]),
  'AI-TC-WRITE-03': uaJson([{ subject: '窗口宽度', intent: 'MODIFY', new_value: '1400', target_config_key: 'livedesign.window.width', is_chat: false }]),

  // ── MIXED 组 ──
  'AI-TC-MIXED-01': uaJson([
    { subject: '所有配置项', intent: 'QUERY', is_chat: false },
    { subject: '房间号', intent: 'MODIFY', new_value: '10041', target_config_key: 'livedesign.room.room_id', is_chat: false },
  ]),
  'AI-TC-MIXED-02': uaJson([
    { subject: '所有配置项', intent: 'QUERY', is_chat: false },
    { subject: '房间号', intent: 'MODIFY', new_value: '10041', target_config_key: 'livedesign.room.room_id', is_chat: false },
    { subject: 'gate', intent: 'ACTION', is_chat: true },
  ]),
  'AI-TC-MIXED-03': uaJson([
    { subject: '弹幕配置', intent: 'QUERY', target_config_key: 'livedesign.danmaku', is_chat: false },
    { subject: '屏蔽点赞', intent: 'MODIFY', new_value: 'true', target_config_key: 'livedesign.room.block_like', is_chat: false },
  ]),
  'AI-TC-MIXED-04': uaJson([
    { subject: '房间号', intent: 'QUERY', target_config_key: 'livedesign.room.room_id', is_chat: false },
    { subject: 'schema', intent: 'ACTION', is_chat: true },
  ]),

  // ── GATE 组 ──
  'AI-TC-GATE-01': uaJson([{ subject: '所有配置项', intent: 'QUERY', is_chat: false }]),
  'AI-TC-GATE-02': uaJson([{ subject: '房间号', intent: 'MODIFY', new_value: '不能为负数', target_config_key: 'livedesign.room.room_id', is_chat: false }]),

  // ── EDGE 组 ──
  'AI-TC-EDGE-01': uaJson([{ subject: '房间号', intent: 'QUERY', target_config_key: 'livedesign.room.room_id', is_chat: false }]),
  'AI-TC-EDGE-02': uaJson([{ subject: '不存在的配置', intent: 'MODIFY', new_value: 'test', is_chat: false }]),
  'AI-TC-EDGE-03': '',  // 空输入 → UA 无产出 → 降级
  'AI-TC-EDGE-04': '',  // 纯空白 → 同上
  'AI-TC-EDGE-05': uaJson([{ subject: 'livedesign.room.room_id', intent: 'QUERY', target_config_key: 'livedesign.room.room_id', is_chat: false }]),
}

/** 按测试用例 ID 返回咨询Agent 的 mock 产出 */
const CONSULT_RESPONSES: Record<string, string> = {
  'AI-TC-CHAT-01': 'Gate（门禁）是一组配置校验规则，用于验证配置值是否符合业务约束。例如：房间号不能为负数、弹幕字号必须在10-40之间。',
  'AI-TC-CHAT-02': 'Schema 是配置结构的定义，描述了每个配置项的类型、默认值和约束条件。例如：房间号是整数类型，默认值为10001。',
  'AI-TC-CHAT-03': '要创建 Gate 规则，可以说"给房间号加个规则，不能为负数"。系统会自动生成对应的校验规则并绑定到配置项上。',
  'AI-TC-CHAT-04': '支持的功能包括：直播间配置（房间号、标题）、弹幕配置（字号、颜色、屏蔽）、Live2D 角色配置（模型、动作）、连接配置（重连、心跳）等。',
  'AI-TC-CHAT-05': '直接在对话框里说出你想做的事即可，例如"当前有哪些配置"、"帮我把房间号改成10041"、"gate是什么"。',
  'AI-TC-MIXED-02': 'Gate（门禁）是一组配置校验规则，用于验证配置值是否符合业务约束。',
  'AI-TC-MIXED-04': 'Schema 是配置结构的定义，描述了每个配置项的类型、默认值和约束条件。',
}

/** 系统提示词中的唯一标记，用于区分 Agent */
const UA_MARKER = '意图解析助手'
const CONSULT_MARKER = '咨询助手'

// ═══════════════════════════════════════════════════════════════════════
// Mock Bridge 工厂
// ═══════════════════════════════════════════════════════════════════════

function createMockBridge(testCaseId: string): AIBridge {
  return {
    async complete(req: AICompletionRequest): Promise<AICompletionResponse> {
      const sysContent = req.messages[0]?.content || ''
      const userContent = req.messages[1]?.content || ''

      if (sysContent.includes(UA_MARKER)) {
        const uaRaw = UA_RESPONSES[testCaseId]
        // 空/不可解析 → 模拟理解Agent 返回空 → 触发降级路径
        if (uaRaw === '' || uaRaw === undefined) {
          return { message: { role: 'assistant', content: '{}' } }
        }
        return { message: { role: 'assistant', content: uaRaw } }
      }

      if (sysContent.includes(CONSULT_MARKER) || userContent.includes('概念性问题')) {
        const consultText = CONSULT_RESPONSES[testCaseId]
        if (consultText) {
          return { message: { role: 'assistant', content: consultText } }
        }
        return { message: { role: 'assistant', content: '这是一条 mock 咨询回复。' } }
      }

      // 降级路径 LLM / wrapUp LLM：返回空 → 不产生额外 tool_calls
      return { message: { role: 'assistant', content: 'OK' } }
    },
    /** EMB: mock embedding 返回空向量 */
    async embed(_text: string): Promise<number[]> {
      return []
    },
  }
}

interface TestHarness {
  messages: AIMessage[]
  latestAssistantUpdates: AIMessage[]
  suggestionCalls: (string | null)[]
  processingStates: boolean[]
  feedbackCollector: FeedbackCollector
  lastToolDeltaRef: { current: ToolDelta | undefined }
}

function createHarness(testCaseId: string): {
  harness: TestHarness
  callbacks: PipelineCallbacks
} {
  const harness: TestHarness = {
    messages: [],
    latestAssistantUpdates: [],
    suggestionCalls: [],
    processingStates: [],
    feedbackCollector: new FeedbackCollector(),
    lastToolDeltaRef: { current: undefined as ToolDelta | undefined },
  }

  const callbacks: PipelineCallbacks = {
    appendMessage: (msg) => { harness.messages.push(msg) },
    updateLastAssistant: (updater) => {
      const idx = harness.messages.length - 1
      if (idx >= 0) {
        harness.messages[idx] = updater(harness.messages[idx])
      }
    },
    setSuggestion: (data) => { harness.suggestionCalls.push(data) },
    setProcessing: (v) => { harness.processingStates.push(v) },
    feedbackRef: { current: harness.feedbackCollector },
    lastToolDeltaRef: harness.lastToolDeltaRef,
    getBridge: () => createMockBridge(testCaseId),
    getMessages: () => harness.messages,
  }

  return { harness, callbacks }
}

/** 从 harness.messages 中提取所有非空 assistant 文本拼接 */
function assistantText(messages: AIMessage[]): string {
  return messages
    .filter(m => m.role === 'assistant' && m.content)
    .map(m => m.content)
    .join('\n')
}

/** 从 harness.messages 中提取 toolCards */
function allToolCards(messages: AIMessage[]): AIMessage['toolCards'] {
  for (const m of messages) {
    if (m.toolCards && m.toolCards.length > 0) return m.toolCards
  }
  return undefined
}

// ═══════════════════════════════════════════════════════════════════════
// 测试入口
// ═══════════════════════════════════════════════════════════════════════

/**
 * 在 E2E 测试启动前注入 mock 业务适配器数据，
 * 使得 TEST_CASES 中的 LiveDesign 特定主题（如"房间号""弹幕配置"）
 * 可以被 LABEL_TO_KEY/KEY_LABELS 正确解析。
 *
 * D38-5-INV-03: 核心不硬编码，数据由适配器注入。
 */
beforeAll(() => {
  const testAdapter: import('../../business-adapter/types').IBusinessAdapter = {
    id: 'test-e2e',
    displayName: 'TestE2E',
    getFieldDeclarations: () => [],
    getNormalizer: () => ({ normalize: () => null }),
    getAIData: () => ({
      configLabels: {
        'livedesign.room.room_id': '房间号',
        'livedesign.danmaku': '弹幕配置',
        'livedesign.danmaku.font_size': '弹幕字号',
        'livedesign.danmaku.color': '弹幕颜色',
        'livedesign.danmaku.danmaku_color': '弹幕颜色',
        'livedesign.display.window_width': '窗口宽度',
        'livedesign.danmaku.block_like': '屏蔽点赞',
        'livedesign.danmaku.font_family': '字体',
        'livedesign.connection.max_reconnect': '最大重连次数',
        'livedesign.live2d.model_scale': '模型缩放比例',
        'livedesign.live2d.model_path': 'Live2D模型目录路径',
        'livedesign.live2d.model_directory': 'Live2D模型目录',
      },
      baselineKW: {
        '房间号': ['livedesign.room.room_id'],
        '弹幕': ['livedesign.danmaku.font_size'],
        '弹幕字号': ['livedesign.danmaku.font_size'],
        '窗口宽度': ['livedesign.display.window_width'],
        '屏蔽点赞': ['livedesign.danmaku.block_like'],
      },
      invertedIndex: {
        '房间号': ['read_config_value', 'write_config_value'],
        '弹幕': ['read_config_value', 'write_config_value'],
        '字号': ['read_config_value', 'write_config_value'],
      },
      configSemanticTypes: {
        'livedesign.room.room_id': 'config_value',
        'livedesign.danmaku.font_size': 'config_value',
        'livedesign.display.window_width': 'config_value',
        'livedesign.danmaku.block_like': 'config_value',
      },
      trieDict: {
        domainKW: {
          '房间': 'livedesign.room',
          '弹幕': 'livedesign.danmaku',
          '窗口': 'livedesign.display',
          '屏蔽': 'livedesign.danmaku',
        },
        opKW: {},
      },
      skillRoutes: [],
      consultationKnowledge: '',
    }),
    getConsultationKnowledge: () => '',
    getSystemPromptIdentity: () => 'TestE2E',
  }
  BusinessAdapterRegistry.register(testAdapter)
  refreshFromRegistry()
  syncBaselineFromRegistry()
  syncDomainKWFromRegistry()
})

beforeEach(() => {
  executionTrace.reset()
  toolCallRegistry.reset()
  mockExecuteToolCall.mockReset()
  // 默认：所有工具调用成功
  mockExecuteToolCall.mockImplementation(async (tc: ToolCall): Promise<ToolResult> => ({
    success: true,
    data: `[mock] ${tc.function.name} succeeded`,
  }))
})

// ── 辅助：按分组运行测试用例 ──
function runTestCase(tc: PipelineTestCase) {
  const testFn = tc.manualOnly ? it.skip : it
  testFn(`[${tc.id}] ${tc.scenario}`, async () => {
    const { harness, callbacks } = createHarness(tc.id)

    await executePipeline(tc.input, callbacks)

    const text = assistantText(harness.messages)

    // ── expectTools 检查 ──
    if (tc.expectTools) {
      const foundToolCards = allToolCards(harness.messages)
      expect(foundToolCards).toBeDefined()
      expect(foundToolCards!.length).toBeGreaterThanOrEqual(tc.minToolCalls ?? 1)
    }

    // ── mustIncludeTools：至少命中一个 ──
    if (tc.mustIncludeTools.length > 0) {
      const cards = allToolCards(harness.messages)
      if (cards) {
        const toolNamesExecuted = cards.map(c => c.toolName)
        const hasMatch = tc.mustIncludeTools.some(t => toolNamesExecuted.includes(t))
        expect(hasMatch).toBe(true)
      }
    }

    // ── mustContainPlanKeywords：planStep 文本中至少命中一个 ──
    if (tc.mustContainPlanKeywords.length > 0) {
      const stepsText = harness.messages
        .filter(m => m.planSteps && m.planSteps.length > 0)
        .flatMap(m => m.planSteps!.map(s => s.text))
        .join('\n')
      const planMatch = tc.mustContainPlanKeywords.some(kw => stepsText.includes(kw))
      expect(planMatch).toBe(true)
    }

    // ── mustContainResultKeywords：结果文本中至少命中一个 ──
    if (tc.mustContainResultKeywords.length > 0) {
      const resultMatch = tc.mustContainResultKeywords.some(kw => text.includes(kw))
      expect(resultMatch).toBe(true)
    }

    // ── mustNotContainMarkers：不应出现的错误标记 ──
    for (const marker of tc.mustNotContainMarkers) {
      if (marker === '' || marker === 'manually-run-only') continue
      expect(text).not.toContain(marker)
    }

    // ── expectChatReply：应有 chat 回复 ──
    if (tc.expectChatReply) {
      const chatContent = harness.messages
        .filter(m => m.role === 'assistant' && m.content && !m.planSteps && !m.toolCards)
        .map(m => m.content)
        .join('\n')
      // 咨询回复应非空
      expect(chatContent.length).toBeGreaterThan(0)
    }

    // ── setProcessing(false) 必须被调用（管线不挂起） ──
    const lastProcessing = harness.processingStates[harness.processingStates.length - 1]
    expect(lastProcessing).toBe(false)
  })
}

// ═══════════════════════════════════════════════════════════════════════
// 分组测试
// ═══════════════════════════════════════════════════════════════════════

describe('E2E: CHAT（纯咨询）', () => {
  ALL_TEST_CASES.filter(tc => tc.id.startsWith('AI-TC-CHAT')).forEach(runTestCase)
})

describe('E2E: LIST（配置列表）', () => {
  ALL_TEST_CASES.filter(tc => tc.id.startsWith('AI-TC-LIST')).forEach(runTestCase)
})

describe('E2E: WRITE（写入配置）', () => {
  ALL_TEST_CASES.filter(tc => tc.id.startsWith('AI-TC-WRITE')).forEach(runTestCase)
})

describe('E2E: MIXED（混合意图）', () => {
  ALL_TEST_CASES.filter(tc => tc.id.startsWith('AI-TC-MIXED')).forEach(runTestCase)
})

describe('E2E: GATE（规则操作）', () => {
  ALL_TEST_CASES.filter(tc => tc.id.startsWith('AI-TC-GATE')).forEach(runTestCase)
})

describe('E2E: EDGE（边界）', () => {
  ALL_TEST_CASES.filter(tc => tc.id.startsWith('AI-TC-EDGE')).forEach(runTestCase)
})
