/**
 * pipeline-test-cases — AI 管线测试集
 *
 * 每条用例模拟用户输入，定义预期行为和最小通过条件。
 * 用于 CI 自动化循环：打开 Editor → 输入对话 → 分析回复 → 修复 → 循环。
 *
 * 用例 ID 格式：AI-TC-{GROUP}-{N}
 * GROUP: CHAT(纯咨询) / LIST(列表) / WRITE(写入) / MIXED(混合意图) / GATE(Gate规则) / EDGE(边界)
 *
 * 专题七 RAG 管线行为变更说明：
 *   - 模糊查询（非精确关键词）命中 ambiguous 主动澄清路径，不产出工具调用
 *   - 精确匹配（configKey / 明确标签）正常产出工具调用
 *   - is_chat 由理解Agent per-item 判别，非整句正则
 *   - resolveLookupIntent 已删除，三路匹配由检索层+路由表替代
 *
 * 场景标记（针对已修复 Bug）：
 *   BUG-SE-01  EXE  Pre-Gate 空对象导致 WRITE 跳过
 *   BUG-SE-02  KEY  UA 混合意图 English subject 未命中 key（已由检索层+路由表替代）
 *   BUG-SE-03  CHAT all_chat 提前 return 未展示 chatAnswer
 *   BUG-SE-04  KBA 咨询Agent 知识库缺"如何操作"列
 */

export interface PipelineTestCase {
  /** 用例 ID */
  id: string
  /** 用例来源 Bug 标记（可选） */
  bugRef?: string
  /** 用户输入文本 */
  input: string
  /** 场景描述（含英文 subTitle） */
  scenario: string
  /** 是否预期有 tool 调用 */
  expectTools: boolean
  /** 预期最少 tool 调用数（null 不检查） */
  minToolCalls: number | null
  /** 必须出现的 tool 名称（至少一个） */
  mustIncludeTools: string[]
  /** 预期 UA 分析出的意图数（包含 chat 项），0 表示不检查 */
  intentCount: number | null
  /** 预期有 chat 回复（is_chat=true 的意图） */
  expectChatReply: boolean
  /** 预期 planStep 文本关键词（至少命中一个） */
  mustContainPlanKeywords: string[]
  /** 预期结果关键词（至少命中一个） */
  mustContainResultKeywords: string[]
  /** 预期不存在的问题标记（除 manually-run-only 外不检查） */
  mustNotContainMarkers: string[]
  /** 是否仅限手工运行（true = CI 跳过） */
  manualOnly: boolean
  /** 备注 */
  note: string
}

// ═══════════════════════════════════════════════════════════════════════
// 第1组：纯咨询（CHAT）— Gateway Q&A
// ═══════════════════════════════════════════════════════════════════════

export const CHAT_CASES: PipelineTestCase[] = [
  {
    id: 'AI-TC-CHAT-01',
    bugRef: 'BUG-SE-03',
    input: 'gate是什么',
    scenario: 'FAQ: gate 定义',
    expectTools: false,
    minToolCalls: null,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: true,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: ['门禁', '配置', '检查'],
    mustNotContainMarkers: ['证据不足', '无 Registry'],
    manualOnly: false,
    note: 'BUG-SE-03 修复：all_chat 需展示 chatAnswer',
  },
  {
    id: 'AI-TC-CHAT-02',
    input: 'schema是什么',
    scenario: 'FAQ: schema 定义',
    expectTools: false,
    minToolCalls: null,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: true,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: ['Schema', '结构', '配置项'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: false,
    note: '',
  },
  {
    id: 'AI-TC-CHAT-03',
    bugRef: 'BUG-SE-04',
    input: '如何创建gate',
    scenario: 'FAQ: gate 创建操作指引',
    expectTools: false,
    minToolCalls: null,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: true,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: ['门禁', '创建', '规则', '字段'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: false,
    note: 'BUG-SE-04 修复：知识库需含"如何操作"列',
  },
  {
    id: 'AI-TC-CHAT-04',
    input: '有哪些功能',
    scenario: 'FAQ: 系统功能总览',
    expectTools: false,
    minToolCalls: null,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: true,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: ['直播间', '弹幕', 'Live2D'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: false,
    note: '',
  },
  {
    id: 'AI-TC-CHAT-05',
    input: '怎么用',
    scenario: 'FAQ: 使用方式',
    expectTools: false,
    minToolCalls: null,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: true,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: ['配置', '操作'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: false,
    note: '',
  },
]

// ═══════════════════════════════════════════════════════════════════════
// 第2组：配置列表（LIST）
// ═══════════════════════════════════════════════════════════════════════

export const LIST_CASES: PipelineTestCase[] = [
  {
    id: 'AI-TC-LIST-01',
    input: '当前有哪些配置',
    scenario: 'LIST: 列出所有配置',
    expectTools: true,
    minToolCalls: 1,
    mustIncludeTools: ['list_configs'],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: ['LIST', '所有配置项'],
    mustContainResultKeywords: [],
    mustNotContainMarkers: ['证据不足', '无 Registry'],
    manualOnly: false,
    note: '',
  },
  {
    id: 'AI-TC-LIST-02',
    input: '查看弹幕配置',
    scenario: 'LIST: 查看特定域配置',
    expectTools: true,
    minToolCalls: 1,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: ['弹幕'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
]

// ═══════════════════════════════════════════════════════════════════════
// 第3组：写入配置（WRITE）
// ═══════════════════════════════════════════════════════════════════════

export const WRITE_CASES: PipelineTestCase[] = [
  {
    id: 'AI-TC-WRITE-01',
    bugRef: 'BUG-SE-01, BUG-SE-02',
    input: '帮我把房间号改成10041',
    scenario: 'WRITE: 单意图修改配置',
    expectTools: true,
    minToolCalls: 2,
    mustIncludeTools: ['write_config_value'],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: ['WRITE', '房间号'],
    mustContainResultKeywords: ['已将', '房间号'],
    mustNotContainMarkers: ['证据不足', '无 Registry', '🚫'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
  {
    id: 'AI-TC-WRITE-02',
    input: '把弹幕字号改为20',
    scenario: 'WRITE: 修改弹幕配置',
    expectTools: true,
    minToolCalls: 2,
    mustIncludeTools: ['write_config_value'],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: ['WRITE'],
    mustContainResultKeywords: ['已将', '弹幕字号'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
  {
    id: 'AI-TC-WRITE-03',
    input: '设置窗口宽度为1400',
    scenario: 'WRITE: 修改窗口配置',
    expectTools: true,
    minToolCalls: 2,
    mustIncludeTools: ['write_config_value'],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: ['WRITE'],
    mustContainResultKeywords: ['已将', '窗口宽度'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
]

// ═══════════════════════════════════════════════════════════════════════
// 第4组：混合意图（MIXED）— 核心回归目标
// ═══════════════════════════════════════════════════════════════════════

export const MIXED_CASES: PipelineTestCase[] = [
  {
    id: 'AI-TC-MIXED-01',
    input: '当前有哪些配置，帮我把房间号改成10041',
    scenario: 'MIXED: LIST + WRITE 双意图',
    expectTools: true,
    minToolCalls: 3,
    mustIncludeTools: ['list_configs', 'write_config_value'],
    intentCount: 2,
    expectChatReply: false,
    mustContainPlanKeywords: ['LIST', 'WRITE'],
    mustContainResultKeywords: ['已列出', '已将'],
    mustNotContainMarkers: ['证据不足', '无 Registry', '🚫'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
  {
    id: 'AI-TC-MIXED-02',
    bugRef: 'BUG-SE-02, BUG-SE-03',
    input: '当前有哪些配置，帮我把房间号改成10041，gate是什么',
    scenario: 'MIXED: LIST + WRITE + CHAT 三意图',
    expectTools: true,
    minToolCalls: 3,
    mustIncludeTools: ['list_configs', 'write_config_value'],
    intentCount: 3,
    expectChatReply: true,
    mustContainPlanKeywords: ['LIST', 'WRITE'],
    mustContainResultKeywords: ['已列出', '已将', '门禁'],
    mustNotContainMarkers: ['证据不足', '无 Registry', '🚫'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
  {
    id: 'AI-TC-MIXED-03',
    input: '查看弹幕配置，把屏蔽点赞打开',
    scenario: 'MIXED: READ + WRITE（模糊主题→回问澄清）',
    expectTools: false,
    minToolCalls: 0,
    mustIncludeTools: [],
    intentCount: 2,
    expectChatReply: false,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: ['未找到'],
    mustNotContainMarkers: [],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
  {
    id: 'AI-TC-MIXED-04',
    input: '房间号是多少，schema是什么',
    scenario: 'MIXED: READ + CHAT 双意图',
    expectTools: true,
    minToolCalls: 1,
    mustIncludeTools: ['read_config_value'],
    intentCount: 2,
    expectChatReply: true,
    mustContainPlanKeywords: ['READ'],
    mustContainResultKeywords: ['Schema', '结构'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
]

// ═══════════════════════════════════════════════════════════════════════
// 第5组：Gate 规则操作（GATE）
// ═══════════════════════════════════════════════════════════════════════

export const GATE_CASES: PipelineTestCase[] = [
  {
    id: 'AI-TC-GATE-01',
    input: '校验配置',
    scenario: 'GATE: validate_config（通用描述→回问澄清）',
    expectTools: false,
    minToolCalls: 0,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: [],
    mustNotContainMarkers: [],
    manualOnly: false,
    note: 'VALIDATE 需具体 configKey，通用"校验配置"无法映射→L1未命中→ASK选择器',
  },
  {
    id: 'AI-TC-GATE-02',
    input: '给房间号加个规则，不能为负数',
    scenario: 'GATE: create gate rule',
    expectTools: true,
    minToolCalls: 1,
    mustIncludeTools: ['update_gate_rule'],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: ['SET_RULE'],
    mustContainResultKeywords: [],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
]

// ═══════════════════════════════════════════════════════════════════════
// 第6组：边界/命令（EDGE）
// ═══════════════════════════════════════════════════════════════════════

export const EDGE_CASES: PipelineTestCase[] = [
  {
    id: 'AI-TC-EDGE-01',
    input: '/room',
    scenario: 'EDGE: 斜杠命令',
    expectTools: true,
    minToolCalls: 1,
    mustIncludeTools: ['read_config_value'],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: ['READ'],
    mustContainResultKeywords: ['已读取'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: true,
    note: 'D38-5: /room skill route 依赖 LiveDesign 业务数据',
  },
  {
    id: 'AI-TC-EDGE-02',
    input: '帮我修改一个不存在的配置',
    scenario: 'EDGE: 不存在的配置',
    expectTools: false,
    minToolCalls: null,
    mustIncludeTools: [],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: [],
    mustNotContainMarkers: [],
    manualOnly: false,
    note: 'L1未命中→ASK选择器（Day36闭环），不静默用subject当key',
  },
  {
    id: 'AI-TC-EDGE-03',
    input: '',
    scenario: 'EDGE: 空输入',
    expectTools: false,
    minToolCalls: 0,
    mustIncludeTools: [],
    intentCount: null,
    expectChatReply: false,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: [],
    mustNotContainMarkers: [],
    manualOnly: false,
    note: '空输入不应触发任何工具调用',
  },
  {
    id: 'AI-TC-EDGE-04',
    input: '   ',
    scenario: 'EDGE: 纯空白输入',
    expectTools: false,
    minToolCalls: 0,
    mustIncludeTools: [],
    intentCount: null,
    expectChatReply: false,
    mustContainPlanKeywords: [],
    mustContainResultKeywords: [],
    mustNotContainMarkers: [],
    manualOnly: false,
    note: '',
  },
  {
    id: 'AI-TC-EDGE-05',
    input: '帮我查livedesign.room.room_id的值',
    scenario: 'EDGE: 用完整 configKey 查询',
    expectTools: true,
    minToolCalls: 1,
    mustIncludeTools: ['read_config_value'],
    intentCount: 1,
    expectChatReply: false,
    mustContainPlanKeywords: ['READ'],
    mustContainResultKeywords: ['已读取'],
    mustNotContainMarkers: ['证据不足'],
    manualOnly: true,
    note: 'D38-5: 依赖 LiveDesign 业务数据',
  },
]

// ═══════════════════════════════════════════════════════════════════════
// 全量测试集
// ═══════════════════════════════════════════════════════════════════════

export const ALL_TEST_CASES: PipelineTestCase[] = [
  ...CHAT_CASES,
  ...LIST_CASES,
  ...WRITE_CASES,
  ...MIXED_CASES,
  ...GATE_CASES,
  ...EDGE_CASES,
]

/** 按组获取 */
export function getCasesByGroup(group: string): PipelineTestCase[] {
  const groups: Record<string, PipelineTestCase[]> = {
    CHAT: CHAT_CASES,
    LIST: LIST_CASES,
    WRITE: WRITE_CASES,
    MIXED: MIXED_CASES,
    GATE: GATE_CASES,
    EDGE: EDGE_CASES,
  }
  return groups[group] || []
}

/** 获取非仅手工运行的用例 */
export function getAutomatedCases(): PipelineTestCase[] {
  return ALL_TEST_CASES.filter(tc => !tc.manualOnly)
}

/** 统计摘要 */
export function getTestSetSummary(): string {
  const automated = getAutomatedCases()
  const manual = ALL_TEST_CASES.filter(tc => tc.manualOnly)
  const bugRefCount = ALL_TEST_CASES.filter(tc => tc.bugRef).length
  return [
    `AI 管线测试集：共 ${ALL_TEST_CASES.length} 条用例`,
    `  自动化可测：${automated.length} 条`,
    `  仅限手工：${manual.length} 条`,
    `  关联 Bug 回溯：${bugRefCount} 处`,
    `  分组：CHAT ${CHAT_CASES.length} | LIST ${LIST_CASES.length} | WRITE ${WRITE_CASES.length} | MIXED ${MIXED_CASES.length} | GATE ${GATE_CASES.length} | EDGE ${EDGE_CASES.length}`,
  ].join('\n')
}
