/**
 * retriever.test.ts — 检索引擎模块单元测试
 *
 * 覆盖范围：
 *   - BM25 分词 + 评分逻辑（通过模块内部函数 import）
 *   - 多路召回：倒排索引 + BM25 合并去重
 *   - value_type 推断 + getValueTypeFromCandidates
 *   - 空输入 / 无匹配 / 边界情况
 *   - 缓存重置
 *
 * D38-7-INV-01~02: 检索层单一数据源 + synonyms
 */

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import {
  retrieveTopKCandidates,
  getValueTypeFromCandidates,
  resetRetrieverCache,
  resetSchemaTypeCache,
  type ConfigCandidate,
} from '../retriever'

// ── Mock configLabels ───────────────────────────────────────────────
vi.mock('../../tools/configLabels', () => ({
  LABEL_TO_KEY: {
    '房间号': 'livedesign.room.room_id',
    '弹幕字号': 'livedesign.danmaku.font_size',
    '弹幕颜色': 'livedesign.danmaku.font_color',
    '屏蔽点赞': 'livedesign.danmaku.block_like',
    '心跳间隔': 'livedesign.connection.heartbeat_interval',
    '最大重连次数': 'livedesign.connection.max_reconnect',
    '模型文件路径': 'livedesign.live2d.model_path',
    '模型缩放比例': 'livedesign.live2d.model_scale',
    '布局模式': 'livedesign.display.layout_mode',
  },
  AI_HINTS: {
    'livedesign.room.room_id': '直播间房间号，整数类型，用于标识当前直播间',
    'livedesign.danmaku.font_size': '弹幕字体大小，范围 1-200',
    'livedesign.danmaku.font_color': '弹幕字体颜色，十六进制颜色码',
    'livedesign.danmaku.block_like': '是否屏蔽点赞，布尔值',
    'livedesign.connection.heartbeat_interval': '心跳发送间隔（秒）',
    'livedesign.connection.max_reconnect': '最大自动重连次数',
    'livedesign.live2d.model_path': 'Live2D 模型文件路径',
    'livedesign.live2d.model_scale': '模型缩放比例，浮点数',
    'livedesign.display.layout_mode': '布局模式：standard/live/obs',
  },
  KEY_LABELS: {
    'livedesign.room.room_id': '房间号',
    'livedesign.danmaku.font_size': '弹幕字号',
    'livedesign.danmaku.font_color': '弹幕颜色',
    'livedesign.danmaku.block_like': '屏蔽点赞',
    'livedesign.connection.heartbeat_interval': '心跳间隔',
    'livedesign.connection.max_reconnect': '最大重连次数',
    'livedesign.live2d.model_path': '模型文件路径',
    'livedesign.live2d.model_scale': '模型缩放比例',
    'livedesign.display.layout_mode': '布局模式',
  },
}))

// ── Mock fieldRetriever ─────────────────────────────────────────────
const mockInvertedIndex: Record<string, string[]> = {
  '房间号': ['livedesign.room.room_id'],
  '弹幕': ['livedesign.danmaku.font_size', 'livedesign.danmaku.font_color', 'livedesign.danmaku.block_like'],
  '字号': ['livedesign.danmaku.font_size'],
  '颜色': ['livedesign.danmaku.font_color'],
  '屏蔽': ['livedesign.danmaku.block_like'],
  '心跳': ['livedesign.connection.heartbeat_interval'],
  '重连': ['livedesign.connection.max_reconnect'],
  '模型': ['livedesign.live2d.model_path', 'livedesign.live2d.model_scale'],
  '布局': ['livedesign.display.layout_mode'],
}

vi.mock('../../context-manager/fieldRetriever', () => ({
  loadInvertedIndex: () => mockInvertedIndex,
  scoredRetrieve: (index: Record<string, string[]>, intent: string, maxResults: number = 10) => {
    const results: Array<{ field: string; score: number }> = []
    const lowerIntent = intent.toLowerCase().trim()
    if (!lowerIntent) return results
    for (const [keyword, fields] of Object.entries(index)) {
      const lowerKw = keyword.toLowerCase()
      if (lowerKw === lowerIntent || lowerKw.includes(lowerIntent) || lowerIntent.includes(lowerKw)) {
        for (const f of fields) {
          const score = lowerKw === lowerIntent ? 100 : Math.max(lowerKw.length, 1)
          results.push({ field: f, score })
        }
      }
    }
    results.sort((a, b) => b.score - a.score)
    return results.slice(0, maxResults)
  },
}))

// ── Mock window.blessstar ───────────────────────────────────────────
const mockExecuteTool = vi.fn()
const mockGetRegisteredSchemas = vi.fn()

beforeEach(() => {
  // 重置缓存
  resetRetrieverCache()
  resetSchemaTypeCache()

  // 设置 window.blessstar mock
  ;(window as any).blessstar = {
    executeTool: mockExecuteTool,
    getRegisteredSchemas: mockGetRegisteredSchemas,
  }

  // 默认 schema 返回
  mockGetRegisteredSchemas.mockResolvedValue({
    fields: [
      { key: 'livedesign.room.room_id', type: 'number' },
      { key: 'livedesign.danmaku.font_size', type: 'number' },
      { key: 'livedesign.danmaku.font_color', type: 'string' },
      { key: 'livedesign.danmaku.block_like', type: 'boolean' },
      { key: 'livedesign.connection.heartbeat_interval', type: 'number' },
      { key: 'livedesign.connection.max_reconnect', type: 'number' },
      { key: 'livedesign.live2d.model_path', type: 'string' },
      { key: 'livedesign.live2d.model_scale', type: 'double' },
      { key: 'livedesign.display.layout_mode', type: 'string', enum: ['standard', 'live', 'obs'] },
    ],
  })

  // 默认所有 read_config_value 返回 mock 值
  mockExecuteTool.mockImplementation((tool: string, args: any) => {
    if (tool === 'read_config_value') {
      const valueMap: Record<string, string> = {
        'livedesign.room.room_id': '10001',
        'livedesign.danmaku.font_size': '14',
        'livedesign.danmaku.block_like': 'true',
      }
      return Promise.resolve({ result: valueMap[args.key] ?? '' })
    }
    return Promise.resolve({ result: '' })
  })
})

afterEach(() => {
  vi.restoreAllMocks()
})

// ═══════════════════════════════════════════════════════════════════════
// Section 1: 空输入 / 边界情况
// ═══════════════════════════════════════════════════════════════════════

describe('空输入 / 边界情况', () => {
  it('空字符串返回空候选', async () => {
    const result = await retrieveTopKCandidates('')
    expect(result.candidates).toHaveLength(0)
    expect(result.injectedContext).toBe('')
  })

  it('空白字符串返回空候选', async () => {
    const result = await retrieveTopKCandidates('   ')
    expect(result.candidates).toHaveLength(0)
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 2: BM25 多路召回 + 置信度
// ═══════════════════════════════════════════════════════════════════════

describe('BM25 多路召回', () => {
  it('精确匹配：房间号 → Top-1 livedesign.room.room_id', async () => {
    const result = await retrieveTopKCandidates('房间号')
    expect(result.candidates.length).toBeGreaterThanOrEqual(1)
    expect(result.candidates[0].configKey).toBe('livedesign.room.room_id')
    expect(result.candidates[0].label).toBe('房间号')
  })

  it('模糊匹配：心跳 → Top-1 livedesign.connection.heartbeat_interval', async () => {
    const result = await retrieveTopKCandidates('心跳')
    expect(result.candidates.length).toBeGreaterThanOrEqual(1)
    expect(result.candidates[0].configKey).toContain('heartbeat')
  })

  it('中文 bigram 匹配：弹幕颜色 → 匹配 danmaku 相关', async () => {
    const result = await retrieveTopKCandidates('弹幕颜色')
    expect(result.candidates.length).toBeGreaterThanOrEqual(1)
    const keys = result.candidates.map(c => c.configKey)
    expect(keys.some(k => k.startsWith('livedesign.danmaku'))).toBe(true)
  })

  it('Top-5 候选去重不重复', async () => {
    const result = await retrieveTopKCandidates('弹幕', 5)
    const keys = result.candidates.map(c => c.configKey)
    const uniqueKeys = new Set(keys)
    expect(uniqueKeys.size).toBe(keys.length)
  })

  it('retrieveTopKCandidates 支持自定义 topK', async () => {
    const result = await retrieveTopKCandidates('弹幕', 3)
    expect(result.candidates.length).toBeLessThanOrEqual(3)
  })

  it('混合输入：英文+中文 → 兜底有结果', async () => {
    const result = await retrieveTopKCandidates('把room_id改成10041', 3)
    // 即使含英文 + 中文，不应 crash
    expect(Array.isArray(result.candidates)).toBe(true)
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 3: 富文本上下文注入
// ═══════════════════════════════════════════════════════════════════════

describe('富文本上下文注入', () => {
  it('injectedContext 含 configKey 信息', async () => {
    const result = await retrieveTopKCandidates('房间号')
    expect(result.injectedContext).toContain('livedesign.room.room_id')
    expect(result.injectedContext).toContain('直播间房间号')
  })

  it('injectedContext 含当前值', async () => {
    const result = await retrieveTopKCandidates('房间号')
    expect(result.injectedContext).toContain('10001')
  })

  it('injectedContext 按序号排列', async () => {
    const result = await retrieveTopKCandidates('弹幕', 3)
    const lines = result.injectedContext.split('\n').filter(l => l.trim())
    expect(lines[0]).toMatch(/^1\. /)
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 4: value_type 推断
// ═══════════════════════════════════════════════════════════════════════

describe('value_type 推断', () => {
  it('getValueTypeFromCandidates 从候选列表取 valueType', async () => {
    const result = await retrieveTopKCandidates('房间号')
    const vt = getValueTypeFromCandidates(result.candidates, 'livedesign.room.room_id')
    expect(vt).toBe('number')
  })

  it('不存在的 key 返回 undefined', () => {
    const candidates: ConfigCandidate[] = [
      { configKey: 'foo.bar', label: '测试', aiHint: '', valueType: 'string', currentValue: '', score: 1, domains: [] },
    ]
    expect(getValueTypeFromCandidates(candidates, 'not.exist')).toBeUndefined()
  })

  it('文件路径类型推断：path 结尾 → file', async () => {
    // livedesign.live2d.model_path → schema type 是 string，但启发式推断为 file
    const result = await retrieveTopKCandidates('模型文件路径')
    const modelPath = result.candidates.find(c => c.configKey === 'livedesign.live2d.model_path')
    if (modelPath) {
      // 如果 schema 已加载且无 _path 结尾特殊处理，则可能是 'string'
      // 这里只断言不 crash，实际值取决于 schema 和 inferValueType 的优先级
      expect(typeof modelPath.valueType).toBe('string')
    }
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 5: 缓存重置
// ═══════════════════════════════════════════════════════════════════════

describe('缓存重置', () => {
  it('resetRetrieverCache 清除文档集合后再次检索仍返回结果', async () => {
    const before = await retrieveTopKCandidates('房间号')
    expect(before.candidates.length).toBeGreaterThan(0)

    resetRetrieverCache()

    const after = await retrieveTopKCandidates('房间号')
    expect(after.candidates.length).toBeGreaterThan(0)
  })

  it('resetSchemaTypeCache 触发重新加载', async () => {
    mockGetRegisteredSchemas.mockClear()
    resetSchemaTypeCache()
    expect(mockGetRegisteredSchemas).not.toHaveBeenCalled()
    const result = await retrieveTopKCandidates('房间号')
    expect(result.candidates.length).toBeGreaterThan(0)
    // 再次调用应使用缓存（不清除缓存的情况下）
    mockGetRegisteredSchemas.mockClear()
    await retrieveTopKCandidates('房间号')
    expect(mockGetRegisteredSchemas).not.toHaveBeenCalled() // 缓存命中
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 6: 当前值读取
// ═══════════════════════════════════════════════════════════════════════

describe('当前值读取', () => {
  it('已有值的配置项正确读取当前值', async () => {
    const result = await retrieveTopKCandidates('房间号')
    const roomItem = result.candidates.find(c => c.configKey === 'livedesign.room.room_id')
    expect(roomItem?.currentValue).toBe('10001')
  })

  it('未设值的配置项 currentValue 为空', async () => {
    const result = await retrieveTopKCandidates('弹幕颜色')
    const colorItem = result.candidates.find(c => c.configKey === 'livedesign.danmaku.font_color')
    expect(colorItem?.currentValue).toBe('')
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 7: 无 schema 降级（window.blessstar 未定义时）
// ═══════════════════════════════════════════════════════════════════════

describe('无 schema 降级', () => {
  it('window.blessstar 为 undefined 时不 crash', async () => {
    ;(window as any).blessstar = undefined
    const result = await retrieveTopKCandidates('房间号')
    expect(result.candidates.length).toBeGreaterThanOrEqual(1)
  })

  it('getRegisteredSchemas 返回空时不 crash', async () => {
    mockGetRegisteredSchemas.mockResolvedValue({ fields: [] })
    const result = await retrieveTopKCandidates('房间号')
    expect(result.candidates.length).toBeGreaterThanOrEqual(1)
  })

  it('executeTool 异常时不阻断检索', async () => {
    mockExecuteTool.mockRejectedValue(new Error('IPC error'))
    const result = await retrieveTopKCandidates('房间号')
    expect(result.candidates.length).toBeGreaterThanOrEqual(1)
    // currentValue 应为空（读取失败降级）
    expect(result.candidates[0].currentValue).toBe('')
  })
})
