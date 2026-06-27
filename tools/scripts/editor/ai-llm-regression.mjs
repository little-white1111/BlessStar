/**
 * ai-llm-regression.mjs — AI 管线 LLM 回归测试
 *
 * 调用真实 DeepSeek API 验证理解Agent 和咨询Agent 的输出质量。
 * 无法被 vitest（jsdom）覆盖，因为需要真实 HTTP 调用。
 *
 * 用法：
 *   node tools/scripts/editor/ai-llm-regression.mjs
 *   node tools/scripts/editor/ai-llm-regression.mjs --group CHAT
 *   node tools/scripts/editor/ai-llm-regression.mjs --verbose
 */

// ═══════════════════════════════════════════════════════════════════════
// 配置
// ═══════════════════════════════════════════════════════════════════════

const DEEPSEEK_BASE = 'https://api.deepseek.com'
const DEEPSEEK_API_KEY = process.env.DEEPSEEK_API_KEY || 'sk-80617b6e51a74d4c951a0ab719fdb771'
const DEEPSEEK_MODEL = 'deepseek-v4-flash'

// 测试时只跑非仅手工用例
const SKIP_CASES = new Set(['AI-TC-EDGE-03', 'AI-TC-EDGE-04'])

const VALID_OPERATIONS = new Set([
  'READ', 'WRITE', 'LIST', 'VALIDATE', 'ADD_FIELD', 'SET_RULE',
  'CREATE_RULE_CHAIN', 'BROWSE', 'SEARCH', 'FIND', 'VIEW_FILE',
  'EXEC', 'DIAGNOSE', 'GENERATE', 'CHAT',
])

const CHAT_KEYWORDS = {
  'AI-TC-CHAT-01': ['门禁', '规则', '校验'],           // gate是什么
  'AI-TC-CHAT-02': ['结构', '配置项', 'Schema'],       // schema是什么
  'AI-TC-CHAT-04': ['直播', '弹幕', '功能'],           // 有哪些功能
  'AI-TC-MIXED-02': ['门禁'],                          // 混合中的chat
  'AI-TC-MIXED-04': ['结构', 'Schema'],                // 混合中的chat
}

// ═══════════════════════════════════════════════════════════════════════
// Prompt 文本（从 TS 文件提取，保持与运行时一致）
// ═══════════════════════════════════════════════════════════════════════

const UNDERSTANDING_AGENT_PROMPT = `你是 LiveDesign 配置系统的意图解析助手。你的唯一职责是将用户描述解析为结构化意图三元组。
不做猜测、不执行操作、不生成回复文本。

用户可能一次说多个意图（用逗号/句号/分号分隔），请逐条解析为独立的 todo 项。

## 操作枚举（15种）
READ       查看/读取已有配置值 | 关键词：查看、当前值是多少、读取
WRITE      修改/设置已有配置值 | 关键词：改成、设为、调整为、修改
LIST       列出全部配置       | 关键词：有哪些配置、所有配置、批量查看
VALIDATE   校验JSON合法性    | 关键词：校验、验证、检查
ADD_FIELD  新增Schema字段    | 关键词：新增字段、创建字段、加配置项
SET_RULE   增/删/改单条Gate规则 | 关键词：加规则、修改规则、删除规则
CREATE_RULE_CHAIN  构建完整Gate链 | 关键词：创建规则链、条件校验
BROWSE     列出目录内容      | 关键词：有哪些文件、列出目录
SEARCH     搜索文件内容      | 关键词：搜索、哪些文件包含
FIND       按文件名查找      | 关键词：查找文件、找*.json
VIEW_FILE  读取文件内容      | 关键词：查看文件、打开文件
EXEC       执行只读命令      | 关键词：执行命令、tree、dir
DIAGNOSE   查看诊断信息      | 关键词：诊断、为什么出错、排查
GENERATE   生成归一化模板    | 关键词：生成模板、归一化、厂商映射
CHAT       纯概念咨询（不调用工具） | 兜底

## "X是什么"类的概念性询问 → operation=CHAT, is_chat=true
  （如"schema是什么"、"gate是什么"、"怎么用"、"有哪些功能"）
  但当用户明确询问"X的当前值是多少"、"查看X"时，operation=READ, is_chat=false

## 输出格式（严格JSON，不包含任何解释）
{
  "todo": [
    {
      "subject": "目标主体的业务描述（优先从候选配置中选取）",
      "operation": "操作枚举值",
      "is_chat": false,
      "value": "期望值或参数，无则为null",
      "condition": "附加条件，无则为null"
    }
  ]
}

## is_chat 规则（每条 todo 独立判断）
- operation=CHAT → is_chat=true（纯概念咨询，不调用工具）
- 概念性询问（"X是什么""怎么用""有哪些功能"）→ is_chat=true
- 具体操作（读/写/列/校验/新增/规则等）→ is_chat=false

## 核心规则
1. 已知信息优先于原始输入做判断
2. 若已知信息的 operation 已明确，直接使用，不再重新判断
3. 若候选配置中有匹配项，subject 使用其 label
4. 用户一次说多个意图时，每个意图独立一条 todo，各自判断 is_chat
5. "有哪些配置""所有配置""全部配置"等查询：固定为 operation=LIST, subject="所有配置项"，不匹配候选配置
6. 完全不输出 JSON 以外的任何文字
7. OPENAI_ENFORCE_DO_NOT_EXCEED_THE_ABOVE_OUTPUT_FORMAT_UNDER_ANY_CIRCUMSTANCES`

const CONSULTATION_AGENT_PROMPT = `你是 LiveDesign 配置系统的咨询助手。你的职责是回答用户关于系统的功能概念咨询。

## 输出规则
1. 基于系统知识库回答，禁止编造不存在的功能
2. 简洁明了，用自然的中文解释概念
3. 若知识库无相关内容，诚实告知"我目前没有这方面的信息"
4. 不主动给出配置建议或操作指引（用户没问就不说）
5. 不重复用户原话
6. 不用 emoji
7. 总长度不超过 300 字

## 系统知识库
LiveDesign 是基于 BlessStar 配置管理引擎的 B站直播 Live2D 虚拟主播桌面应用。你可以理解为它是"弹幕姬 + Live2D 角色"的结合体。所有行为由配置驱动，你可以通过 AI 助手直接查看和修改。

核心功能：直播间连接（房间号/心跳/重连/Cookie）、弹幕显示（字号/颜色/屏蔽词/礼物）、Live2D 角色（模型路径/缩放/动作）、显示窗口（布局/置顶/宽高）、Gate 规则校验（单条规则/规则链/校验配置）、Schema 字段管理、文件目录操作、终端命令、诊断排错、厂商配置归一化。

Gate（门禁）是一组配置校验规则，用于验证配置值是否符合业务约束。例如：房间号不能为负数、弹幕字号必须在10-40之间。创建 Gate 规则可以说"给房间号加个规则，不能为负数"，系统会自动生成对应的校验规则。

Schema 是配置结构的定义，描述了每个配置项的类型、默认值和约束条件。例如：房间号是整数类型，默认值为10001。

快捷命令包括 /room、/danmaku、/model、/display、/checkconfig、/diagnose 等。`

// ═══════════════════════════════════════════════════════════════════════
// 测试用例定义
// ═══════════════════════════════════════════════════════════════════════

/**
 * @typedef {Object} LLMTestCase
 * @property {string} id
 * @property {string} group
 * @property {string} input
 * @property {string} scenario
 * @property {number} minTodoItems - 最少 todo 条数
 * @property {Array<{subject?: string, operation?: string, is_chat?: boolean}>} expectedItems - 预期 todo（可部分匹配）
 * @property {boolean} expectChatReply - 是否需额外咨询Agent 调用
 * @property {string[]} expectConsultKeywords - 咨询回复关键词
 */

/** @type {LLMTestCase[]} */
const TEST_CASES = [
  // ── CHAT ──
  { id: 'AI-TC-CHAT-01', group: 'CHAT', input: 'gate是什么', scenario: 'FAQ: gate',
    minTodoItems: 1, expectedItems: [{ operation: 'CHAT', is_chat: true }],
    expectChatReply: true, expectConsultKeywords: CHAT_KEYWORDS['AI-TC-CHAT-01'] },

  { id: 'AI-TC-CHAT-02', group: 'CHAT', input: 'schema是什么', scenario: 'FAQ: schema',
    minTodoItems: 1, expectedItems: [{ operation: 'CHAT', is_chat: true }],
    expectChatReply: true, expectConsultKeywords: CHAT_KEYWORDS['AI-TC-CHAT-02'] },

  { id: 'AI-TC-CHAT-03', group: 'CHAT', input: '如何创建gate', scenario: 'FAQ: gate 创建',
    minTodoItems: 1, expectedItems: [{ operation: 'CHAT', is_chat: true }],
    expectChatReply: true, expectConsultKeywords: ['门禁', '规则'] },

  { id: 'AI-TC-CHAT-04', group: 'CHAT', input: '有哪些功能', scenario: 'FAQ: 功能总览',
    minTodoItems: 1, expectedItems: [{ operation: 'CHAT', is_chat: true }],
    expectChatReply: true, expectConsultKeywords: CHAT_KEYWORDS['AI-TC-CHAT-04'] },

  { id: 'AI-TC-CHAT-05', group: 'CHAT', input: '怎么用', scenario: 'FAQ: 使用方式',
    minTodoItems: 1, expectedItems: [{ operation: 'CHAT', is_chat: true }],
    expectChatReply: true, expectConsultKeywords: [] },

  // ── LIST ──
  { id: 'AI-TC-LIST-01', group: 'LIST', input: '当前有哪些配置', scenario: 'LIST 所有配置',
    minTodoItems: 1, expectedItems: [{ operation: 'LIST', is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-LIST-02', group: 'LIST', input: '查看弹幕配置', scenario: 'LIST 弹幕配置',
    minTodoItems: 1, expectedItems: [{ is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  // ── WRITE ──
  { id: 'AI-TC-WRITE-01', group: 'WRITE', input: '帮我把房间号改成10041', scenario: 'WRITE 房间号',
    minTodoItems: 1, expectedItems: [{ operation: 'WRITE', is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-WRITE-02', group: 'WRITE', input: '把弹幕字号改为20', scenario: 'WRITE 弹幕字号',
    minTodoItems: 1, expectedItems: [{ operation: 'WRITE', is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-WRITE-03', group: 'WRITE', input: '设置窗口宽度为1400', scenario: 'WRITE 窗口宽度',
    minTodoItems: 1, expectedItems: [{ operation: 'WRITE', is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  // ── MIXED ──
  { id: 'AI-TC-MIXED-01', group: 'MIXED', input: '当前有哪些配置，帮我把房间号改成10041', scenario: 'LIST+WRITE',
    minTodoItems: 2, expectedItems: [
      { operation: 'LIST', is_chat: false },
      { operation: 'WRITE', is_chat: false },
    ],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-MIXED-02', group: 'MIXED', input: '当前有哪些配置，帮我把房间号改成10041，gate是什么', scenario: 'LIST+WRITE+CHAT',
    minTodoItems: 3, expectedItems: [
      { operation: 'LIST', is_chat: false },
      { operation: 'WRITE', is_chat: false },
      { operation: 'CHAT', is_chat: true },
    ],
    expectChatReply: true, expectConsultKeywords: CHAT_KEYWORDS['AI-TC-MIXED-02'] },

  { id: 'AI-TC-MIXED-03', group: 'MIXED', input: '查看弹幕配置，把屏蔽点赞打开', scenario: 'READ+WRITE',
    minTodoItems: 2, expectedItems: [
      { is_chat: false },
      { is_chat: false },
    ],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-MIXED-04', group: 'MIXED', input: '房间号是多少，schema是什么', scenario: 'READ+CHAT',
    minTodoItems: 2, expectedItems: [
      { is_chat: false },        // READ 房间号
      { operation: 'CHAT', is_chat: true },  // CHAT schema
    ],
    expectChatReply: true, expectConsultKeywords: CHAT_KEYWORDS['AI-TC-MIXED-04'] },

  // ── GATE ──
  { id: 'AI-TC-GATE-01', group: 'GATE', input: '校验配置', scenario: 'VALIDATE',
    minTodoItems: 1, expectedItems: [{ operation: 'VALIDATE', is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-GATE-02', group: 'GATE', input: '给房间号加个规则，不能为负数', scenario: 'SET_RULE',
    minTodoItems: 1, expectedItems: [{ operation: 'SET_RULE', is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  // ── EDGE ──
  { id: 'AI-TC-EDGE-01', group: 'EDGE', input: '/room', scenario: '斜杠命令',
    minTodoItems: 1, expectedItems: [{ is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-EDGE-02', group: 'EDGE', input: '帮我修改一个不存在的配置', scenario: '不存在的配置',
    minTodoItems: 1, expectedItems: [{ is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },

  { id: 'AI-TC-EDGE-05', group: 'EDGE', input: '帮我查livedesign.room.room_id的值', scenario: '完整 configKey',
    minTodoItems: 1, expectedItems: [{ is_chat: false }],
    expectChatReply: false, expectConsultKeywords: [] },
]

// ═══════════════════════════════════════════════════════════════════════
// DeepSeek API 调用
// ═══════════════════════════════════════════════════════════════════════

/**
 * @param {Array<{role: string, content: string}>} messages
 * @param {{ temperature?: number }} [opts]
 * @returns {Promise<string>}
 */
async function callDeepSeek(messages, opts = {}) {
  const body = {
    model: DEEPSEEK_MODEL,
    messages,
    stream: false,
    temperature: opts.temperature ?? 0.1,  // 低温度 = 更确定性
  }

  const res = await fetch(`${DEEPSEEK_BASE}/chat/completions`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${DEEPSEEK_API_KEY}`,
    },
    body: JSON.stringify(body),
  })

  if (!res.ok) {
    const errText = await res.text()
    throw new Error(`DeepSeek API ${res.status}: ${errText.slice(0, 300)}`)
  }

  const data = await res.json()
  return data.choices?.[0]?.message?.content || ''
}

// ═══════════════════════════════════════════════════════════════════════
// JSON 提取（容错处理）
// ═══════════════════════════════════════════════════════════════════════

/**
 * 从 LLM 输出中提取 JSON（支持 markdown code block 包装）
 * @param {string} raw
 * @returns {object|null}
 */
function extractJson(raw) {
  // 尝试直接解析
  try { return JSON.parse(raw.trim()) } catch { /* ok */ }

  // 尝试提取 ```json ... ``` 中的内容
  const mdMatch = raw.match(/```(?:json)?\s*\n?([\s\S]*?)```/)
  if (mdMatch) {
    try { return JSON.parse(mdMatch[1].trim()) } catch { /* ok */ }
  }

  // 尝试提取 { ... } 块
  const braceMatch = raw.match(/\{[\s\S]*"todo"[\s\S]*\}/)
  if (braceMatch) {
    try { return JSON.parse(braceMatch[0]) } catch { /* ok */ }
  }

  return null
}

// ═══════════════════════════════════════════════════════════════════════
// 单条用例测试
// ═══════════════════════════════════════════════════════════════════════

/**
 * @typedef {Object} TestResult
 * @property {string} id
 * @property {boolean} passed
 * @property {string[]} errors
 * @property {string[]} warnings
 * @property {*} uaOutput - 理解Agent 解析后的 JSON
 * @property {string|null} consultReply - 咨询Agent 回复（如有）
 */

/**
 * @param {LLMTestCase} tc
 * @param {boolean} verbose
 * @returns {Promise<TestResult>}
 */
async function runCase(tc, verbose) {
  const errors = []
  const warnings = []

  if (verbose) process.stdout.write(`  ${tc.id} [${tc.group}] ${tc.scenario}... `)

  try {
    // ── Step A: 理解Agent ──
    const uaRaw = await callDeepSeek([
      { role: 'system', content: UNDERSTANDING_AGENT_PROMPT },
      { role: 'user', content: `## 用户描述\n${tc.input}` },
    ])

    const uaOutput = extractJson(uaRaw)
    if (!uaOutput) {
      errors.push(`UA JSON 解析失败: ${uaRaw.slice(0, 120)}`)
      if (verbose) console.log('FAIL (JSON parse)')
      return { id: tc.id, passed: false, errors, warnings, uaOutput: null, consultReply: null }
    }

    if (!Array.isArray(uaOutput.todo)) {
      errors.push('UA 输出无 todo 数组')
      if (verbose) console.log('FAIL (no todo)')
      return { id: tc.id, passed: false, errors, warnings, uaOutput, consultReply: null }
    }

    const items = uaOutput.todo

    // ── 结构校验 ──
    if (items.length < tc.minTodoItems) {
      errors.push(`todo 条数不足：期望 ≥${tc.minTodoItems}，实际 ${items.length}`)
    }

    for (let i = 0; i < items.length; i++) {
      const item = items[i]
      if (!item.subject || typeof item.subject !== 'string' || item.subject.trim() === '') {
        errors.push(`todo[${i}].subject 为空`)
      }
      if (!item.operation || !VALID_OPERATIONS.has(item.operation)) {
        errors.push(`todo[${i}].operation="${item.operation}" 无效（合法值：${[...VALID_OPERATIONS].join('|')}）`)
      }
      if (item.is_chat === undefined || item.is_chat === null) {
        warnings.push(`todo[${i}] 缺少 is_chat 字段（默认 false）`)
      }
    }

    // ── 语义校验：预期项匹配 ──
    for (const expected of tc.expectedItems) {
      let matched = false
      for (const item of items) {
        if (expected.operation && item.operation !== expected.operation) continue
        if (expected.is_chat !== undefined && item.is_chat !== expected.is_chat) continue
        if (expected.subject && !item.subject?.includes(expected.subject)) continue
        matched = true
        break
      }
      if (!matched) {
        const desc = expected.operation
          ? `operation=${expected.operation} is_chat=${expected.is_chat}`
          : `is_chat=${expected.is_chat}`
        errors.push(`预期 ${desc} 未在 ${items.map(i => `[${i.operation} ${i.subject}]`).join(', ')} 中匹配`)
      }
    }

    // ── 额外检查：CHAT operation 必须 is_chat=true ──
    for (const item of items) {
      if (item.operation === 'CHAT' && item.is_chat !== true) {
        errors.push(`todo [${item.operation} ${item.subject}] 的 is_chat 应为 true，实际为 ${item.is_chat}`)
      }
      if (item.operation !== 'CHAT' && item.is_chat === true) {
        errors.push(`todo [${item.operation} ${item.subject}] is_chat=true 但 operation 非 CHAT`)
      }
    }

    // ── Step B: 咨询Agent（仅 CHAT 用例）──
    let consultReply = null
    if (tc.expectChatReply && errors.length === 0) {
      const chatQuestions = items
        .filter(i => i.operation === 'CHAT')
        .map(i => i.subject)
        .join('\n')

      if (chatQuestions) {
        consultReply = await callDeepSeek([
          { role: 'system', content: CONSULTATION_AGENT_PROMPT },
          { role: 'user', content: `用户问了以下概念性问题：\n${chatQuestions}\n\n请分别解答。` },
        ])

        if (tc.expectConsultKeywords && tc.expectConsultKeywords.length > 0) {
          const hitCount = tc.expectConsultKeywords.filter(kw => consultReply.includes(kw)).length
          if (hitCount === 0) {
            warnings.push(`咨询回复未命中关键词：${tc.expectConsultKeywords.join('/')} → "${consultReply.slice(0, 100)}"`)
          } else if (hitCount < tc.expectConsultKeywords.length) {
            const missed = tc.expectConsultKeywords.filter(kw => !consultReply.includes(kw))
            warnings.push(`咨询回复部分命中（${hitCount}/${tc.expectConsultKeywords.length}），缺：${missed.join('/')}`)
          }
        }
      }
    }

    const passed = errors.length === 0
    if (verbose) console.log(passed ? 'PASS' : 'FAIL')
    if (!passed && verbose) {
      for (const e of errors) console.log(`        ✗ ${e}`)
      for (const w of warnings) console.log(`        ⚠ ${w}`)
    }

    return { id: tc.id, passed, errors, warnings, uaOutput, consultReply }
  } catch (err) {
    errors.push(`API 异常: ${err.message}`)
    if (verbose) console.log('FAIL (API error)')
    return { id: tc.id, passed: false, errors, warnings, uaOutput: null, consultReply: null }
  }
}

// ═══════════════════════════════════════════════════════════════════════
// 主入口
// ═══════════════════════════════════════════════════════════════════════

const args = process.argv.slice(2)
const verbose = args.includes('--verbose')
const filterGroup = args.includes('--group') ? args[args.indexOf('--group') + 1] : null

let cases = TEST_CASES
if (filterGroup) {
  cases = TEST_CASES.filter(tc => tc.group === filterGroup)
  if (cases.length === 0) {
    console.log(`未找到分组: ${filterGroup}。可用：CHAT LIST WRITE MIXED GATE EDGE`)
    process.exit(1)
  }
}

console.log('')
console.log('╔══════════════════════════════════════════════════════╗')
console.log('║   AI 管线 LLM 回归测试 — 真实 DeepSeek API           ║')
console.log(`║   model: ${DEEPSEEK_MODEL.padEnd(34)}║`)
console.log(`║   cases: ${String(cases.length).padEnd(34)}║`)
console.log('╚══════════════════════════════════════════════════════╝')
console.log('')

const startTime = Date.now()
const results = []

// 串行执行（避免 API 限流）
for (const tc of cases) {
  if (SKIP_CASES.has(tc.id)) {
    if (verbose) console.log(`  ${tc.id} 跳过（仅限手工）`)
    continue
  }
  const result = await runCase(tc, verbose)
  results.push(result)
  // 微延迟避免限流
  await new Promise(r => setTimeout(r, 300))
}

// ── 汇总 ──
const passed = results.filter(r => r.passed).length
const failed = results.filter(r => !r.passed).length
const warnings = results.filter(r => r.warnings.length > 0).length
const elapsed = ((Date.now() - startTime) / 1000).toFixed(1)

console.log('')
console.log('═══════════════════════════════════════════════════════')
console.log(`  Results: ${passed} passed, ${failed} failed, ${warnings} warnings (${elapsed}s)`)
console.log('═══════════════════════════════════════════════════════')

// ── 详细结果 ──
const failedCases = results.filter(r => !r.passed)
if (failedCases.length > 0) {
  console.log('')
  console.log('── 失败用例 ──────────────────────────────────────────')
  for (const r of failedCases) {
    // find the test case
    const tc = cases.find(c => c.id === r.id)
    console.log(`  ✗ ${r.id} (${tc?.group || '?'}) ${tc?.scenario || ''}`)
    for (const e of r.errors) console.log(`      ⛔ ${e}`)
    if (r.uaOutput) {
      console.log(`      UA 产出: ${JSON.stringify(r.uaOutput).slice(0, 200)}`)
    }
  }
}

const warnCases = results.filter(r => r.warnings.length > 0 && r.passed)
if (warnCases.length > 0) {
  console.log('')
  console.log('── 警告用例 ──────────────────────────────────────────')
  for (const r of warnCases) {
    const tc = cases.find(c => c.id === r.id)
    console.log(`  ⚠ ${r.id} (${tc?.group || '?'})`)
    for (const w of r.warnings) console.log(`      ${w}`)
  }
}

// ── 分组统计 ──
const groups = {}
for (const r of results) {
  const tc = cases.find(c => c.id === r.id)
  const g = tc?.group || '?'
  if (!groups[g]) groups[g] = { passed: 0, failed: 0 }
  if (r.passed) groups[g].passed++
  else groups[g].failed++
}

console.log('')
console.log('── 分组 ──────────────────────────────────────────────')
for (const [group, stats] of Object.entries(groups)) {
  const icon = stats.failed === 0 ? '✅' : '❌'
  console.log(`  ${icon} ${group.padEnd(8)} ${stats.passed} pass / ${stats.failed} fail`)
}

console.log('')

if (failed > 0) {
  process.exit(1)
} else {
  process.exit(0)
}
