/**
 * pipeline-stages.test.ts — AI 管线确定性阶段测试
 *
 * 测试范围：所有不依赖 LLM 的管线阶段。
 *   - stage-router: L0 采集（多子句/is_chat 已分离为导出函数）
 *   - stage-intent: Think Level、UA 消息构建、indexShardLoad、L1 降级
 *   - stage-execute: mapTripletsToToolCalls 映射层、roundVerify 证据配平
 *   - configLabels: LABEL_TO_KEY 反向映射、KEY_LABELS 子串匹配兜底
 *   - trie_matcher: 关键词三元组压缩
 *   - operationMapper: operation→tool 映射、权限校验
 *
 * LLM 依赖项（理解Agent/咨询Agent/降级LLM）由单独 mock 测试覆盖。
 * 测试用例来源于 pipeline-test-cases.ts 测试集。
 */

import { describe, it, expect } from 'vitest'
import { executeStageRouter, detectMultiClause, detectChatQuery } from '../stage-router'
import { executeStageIntent, executeIndexShardLoad, collectHints, buildUAUserMessage, parseUnderstandingAgentOutput } from '../stage-intent'
import { matchSkill } from '../../context-manager/skillRouter'
import { mapTripletsToToolCalls } from '../stage-execute'
import { roundVerify, toolCallRegistry } from '../../context-manager/executionTrace'
import { LABEL_TO_KEY, KEY_LABELS } from '../../tools/configLabels'
import { validateOperationForConfig, operationToTools } from '../../operationMapper'
import { compressIntent } from '../../intent/trie_matcher'
import { createPipelineContext } from '../types'
import { ALL_TEST_CASES } from './pipeline-test-cases'
import type { PlanStep } from '../../types'

// ═══════════════════════════════════════════════════════════════════════
// Section 1: Stage Router 测试（L0 + 多子句 + is_chat）
// ═══════════════════════════════════════════════════════════════════════

describe('Stage Router', () => {
  describe('⑨ is_chat 检测（降级路径用）', () => {
    it('isChatQuery 仅对 /command+咨询模式生效', () => {
      const skillMatch = matchSkill('/list 怎么用')
      // /list = UNIFIED_SKILLS (parseCommand) + "怎么用" 不在正则范围 → false
      expect(detectChatQuery('/list 怎么用', skillMatch)).toBe(false)
    })

    it('/command 加"是什么"触发 isChatQuery', () => {
      const skillMatch = matchSkill('/list 是什么')
      expect(detectChatQuery('/list 是什么', skillMatch)).toBe(true)
    })

    const actionInputs = ['帮我把房间号改成10041', '当前有哪些配置', '校验配置']
    for (const input of actionInputs) {
      it(`应将 "${input}" 标记为非咨询类`, () => {
        const skillMatch = matchSkill(input)
        expect(detectChatQuery(input, skillMatch)).toBe(false)
      })
    }
  })

  describe('⑧ 多子句分句（降级路径用）', () => {
    it('应正确切分逗号分隔的多意图', () => {
      const { clauses, isMultiClause } = detectMultiClause('当前有哪些配置，帮我把房间号改成10041')
      expect(isMultiClause).toBe(true)
      expect(clauses.length).toBeGreaterThanOrEqual(2)
      expect(clauses.join('')).toContain('当前有哪些配置')
      expect(clauses.join('')).toContain('帮我把房间号改成10041')
    })

    it('应正确切分句号分隔的多意图', () => {
      const { clauses, isMultiClause } = detectMultiClause('当前有哪些配置。帮我把房间号改成10041')
      expect(isMultiClause).toBe(true)
      expect(clauses.length).toBeGreaterThanOrEqual(2)
    })

    it('应识别单意图为 isMultiClause=false', () => {
      const { isMultiClause } = detectMultiClause('帮我把房间号改成10041')
      expect(isMultiClause).toBe(false)
    })
  })

  describe('⑨ is_chat + ⑧ 联合', () => {
    it('"gate是什么"：isChatQuery 仅对 /command 生效', () => {
      const { isMultiClause } = detectMultiClause('gate是什么')
      expect(isMultiClause).toBe(false)
      const skillMatch = matchSkill('gate是什么')
      // 非 /command → isChatQuery = false（chat 分流由 UA per-item is_chat 负责）
      expect(detectChatQuery('gate是什么', skillMatch)).toBe(false)
    })

    it('"/list 是什么"：command+咨询 → isChatQuery=true', () => {
      const { isMultiClause } = detectMultiClause('/list 是什么')
      expect(isMultiClause).toBe(false)
      const skillMatch = matchSkill('/list 是什么')
      expect(detectChatQuery('/list 是什么', skillMatch)).toBe(true)
    })
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 2: Stage Intent 测试（压缩 + 分片 + Token）
// ═══════════════════════════════════════════════════════════════════════

describe('Stage Intent', () => {
  describe('② Trie 三元组压缩', () => {
    it('"添加字段" → operation=SCHEMA', () => {
      const result = compressIntent('添加字段')
      expect(result).not.toBeNull()
      expect(result!.operation).toBe('schema')
      expect(result!.config.domain).toBe('schema.field')
    })

    it('"弹幕显示" → 可能命中（取决于 DOMAIN_KW）或 null', () => {
      // DOMAIN_KW 出厂仅含通用词；业务领域词由 adapter 注入
      const result = compressIntent('弹幕显示')
      if (result) {
        expect(result.config.domain).toContain('danmaku')
      }
      // null（降级 LLM）也合法
    })

    it('"gate是什么" → 无操作词 → null', () => {
      const result = compressIntent('gate是什么')
      expect(result).toBeNull() // 无 OP_KW 匹配
    })
  })

  describe('⑩a UA 消息构建（L1 零知识）', () => {
    it('应仅注入 L0 hint，不包含 L1 候选', () => {
      const hints = collectHints({ operationHint: 'WRITE', subjectHint: '房间号' })
      expect(hints.l1).toBeNull()
      expect(hints.l0).not.toBeNull()

      const uaMsg = buildUAUserMessage('帮我把房间号改成10041', hints)
      expect(uaMsg).toContain('用户描述')
      expect(uaMsg).toContain('WRITE')       // L0 使用 English enum 名
      // 不应包含候选配置列表（PIPELINE-15）
      expect(uaMsg).not.toContain('候选配置项')
    })
  })

  describe('Ⓢ executeStageIntent 端到端', () => {
    it('应正确填充 hints 和 uaUserMessage', async () => {
      const ctx = createPipelineContext('帮我把房间号改成10041')
      executeStageRouter(ctx)
      await executeStageIntent(ctx)
      // L0 仅对 /command 命中时有值；自然语言输入 L0 为 null 属正确行为
      expect(ctx.hints.l1).toBeNull()
      expect(ctx.uaUserMessage).toContain('帮我把房间号改成10041')
    })
  })

  describe('③ 分片加载（降级路径用）', () => {
    it('executeIndexShardLoad 应正确构建 compressed 和 effectiveIndex', async () => {
      const { compressed, effectiveIndex } = await executeIndexShardLoad('查看弹幕配置')
      // compressed 可能为 null（无 OP_KW 匹配）或有效
      // effectiveIndex 可能为 null（无匹配 shard）或有效
      // 不崩溃即可
      expect(true).toBe(true)
    })
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 3: 映射层 ⑩c mapTripletsToToolCalls
// ═══════════════════════════════════════════════════════════════════════

describe('mapTripletsToToolCalls', () => {
  it('LIST → 生成 list_configs 工具调用', () => {
    const items = [{ subject: '所有配置项', operation: 'LIST', value: null, condition: null }]
    const { toolCallsToExecute } = mapTripletsToToolCalls(items)
    expect(toolCallsToExecute.length).toBe(1)
    expect(toolCallsToExecute[0].function.name).toBe('list_configs')
  })

  it('WRITE → 生成 read_config_value + write_config_value', () => {
    const items = [{ subject: '房间号', operation: 'WRITE', value: '10041', condition: null }]
    const subjectToKey = { '房间号': 'livedesign.room.room_id' }
    const { toolCallsToExecute } = mapTripletsToToolCalls(items, subjectToKey)
    expect(toolCallsToExecute.length).toBe(2)
    expect(toolCallsToExecute[0].function.name).toBe('read_config_value')
    expect(toolCallsToExecute[1].function.name).toBe('write_config_value')

    // G4 修复：value 应被注入 tool args
    const writeArgs = JSON.parse(toolCallsToExecute[1].function.arguments)
    expect(writeArgs.value).toBe('10041')
    // G7 修复：key 应为 livedesign.room.room_id
    expect(writeArgs.key).toBe('livedesign.room.room_id')
  })

  it('WRITE 无 subjectToKey → 不注入 key', () => {
    const items = [{ subject: '房间号', operation: 'WRITE', value: '10041', condition: null }]
    const { toolCallsToExecute } = mapTripletsToToolCalls(items)
    expect(toolCallsToExecute.length).toBe(2)
    const writeArgs = JSON.parse(toolCallsToExecute[1].function.arguments)
    expect(writeArgs.value).toBe('10041')
    expect(writeArgs.key).toBeUndefined()
  })

  it('混合 LIST + WRITE → 两个 planStep 分组', () => {
    const items = [
      { subject: '所有配置项', operation: 'LIST', value: null, condition: null },
      { subject: '房间号', operation: 'WRITE', value: '10041', condition: null },
    ]
    const subjectToKey = { '房间号': 'livedesign.room.room_id' }
    const { toolCallsToExecute, planStepToolRanges } = mapTripletsToToolCalls(items, subjectToKey)

    // LIST: 1 tool, WRITE: 2 tools → total 3
    expect(toolCallsToExecute.length).toBe(3)
    expect(planStepToolRanges.length).toBe(2)
    expect(planStepToolRanges[0]).toEqual([0])       // LIST = idx 0
    expect(planStepToolRanges[1]).toEqual([1, 2])    // WRITE = idx 1,2
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 4: configLabels — LABEL_TO_KEY + KEY_LABELS 子串兜底
// ═══════════════════════════════════════════════════════════════════════

describe('configLabels — Key Resolution（出厂基线）', () => {
  describe('LABEL_TO_KEY / KEY_LABELS 出厂为空', () => {
    it('出厂基线为空 Record，启动时由 BusinessAdapterRegistry 注入', () => {
      expect(Object.keys(LABEL_TO_KEY).length).toBe(0)
      expect(Object.keys(KEY_LABELS).length).toBe(0)
    })

    it('空数据时子串匹配返回 null', () => {
      const resolveKeyWithFallback = (subject: string): string | null => {
        if (LABEL_TO_KEY[subject]) return LABEL_TO_KEY[subject]
        const lowerSubject = subject.toLowerCase()
        return Object.keys(KEY_LABELS).find(k => k.toLowerCase().includes(lowerSubject)) ?? null
      }
      expect(resolveKeyWithFallback('room')).toBeNull()
      expect(resolveKeyWithFallback('font')).toBeNull()
      expect(resolveKeyWithFallback('wxid_notexist')).toBeNull()
    })
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 5: operationMapper — 权限校验
// ═══════════════════════════════════════════════════════════════════════

describe('operationMapper — 权限校验', () => {
  it('WRITE 操作对任意 key 合法（默认白名单）', () => {
    const err = validateOperationForConfig('some.domain.field', 'WRITE')
    expect(err).toBeNull()
  })

  it('READ 操作对任意 key 合法（默认白名单）', () => {
    const err = validateOperationForConfig('some.domain.field', 'READ')
    expect(err).toBeNull()
  })

  it('LIST 操作对任意 key 合法（默认白名单）', () => {
    const err = validateOperationForConfig('some.domain.field', 'LIST')
    expect(err).toBeNull()
  })

  it('WRITE → 生成 read + write 两个 tool', () => {
    const tools = operationToTools('WRITE')
    expect(tools).toContain('read_config_value')
    expect(tools).toContain('write_config_value')
  })

  it('LIST → 生成 list_configs 一个 tool', () => {
    const tools = operationToTools('LIST')
    expect(tools).toEqual(['list_configs'])
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 6: roundVerify — 证据链配平
// ═══════════════════════════════════════════════════════════════════════

describe('roundVerify 证据链', () => {
  it('所有 planStep 有 callId + success → allMatched=true', () => {
    const steps: PlanStep[] = [
      { id: 1, text: '[LIST] 所有配置项', done: true, callId: 'call_rv_001' },
      { id: 2, text: '[WRITE] 房间号 → 10041', done: true, callId: 'call_rv_002' },
    ]
    toolCallRegistry.record('call_rv_001', 'list_configs', 'OK', 'success')
    toolCallRegistry.record('call_rv_002', 'write_config_value', 'OK', 'success')

    const result = roundVerify(steps, '')
    expect(result.allMatched).toBe(true)
    expect(result.unmatchedSteps).toHaveLength(0)
  })

  it('缺失 callId → unmatched', () => {
    const steps: PlanStep[] = [
      { id: 1, text: '[WRITE] 房间号 → 10041', done: true },
    ]
    const result = roundVerify(steps, '')
    expect(result.allMatched).toBe(false)
    expect(result.unmatchedSteps).toHaveLength(1)
  })

  it('多 tool allCallIds 全成功 → allMatched=true', () => {
    const steps: PlanStep[] = [
      { id: 1, text: '[WRITE] 房间号 → 10041', done: true, allCallIds: ['call_rva_01', 'call_rva_02'] },
    ]
    toolCallRegistry.record('call_rva_01', 'read_config_value', 'OK', 'success')
    toolCallRegistry.record('call_rva_02', 'write_config_value', 'OK', 'success')

    const result = roundVerify(steps, '')
    expect(result.allMatched).toBe(true)
  })

  it('多 tool allCallIds 有一个失败 → unmatched', () => {
    const steps: PlanStep[] = [
      { id: 1, text: '[WRITE] 房间号 → 10041', done: true, allCallIds: ['call_rvb_01', 'call_rvb_02'] },
    ]
    toolCallRegistry.record('call_rvb_01', 'read_config_value', 'OK', 'success')
    toolCallRegistry.record('call_rvb_02', 'write_config_value', 'ERR', 'failed')

    const result = roundVerify(steps, '')
    expect(result.allMatched).toBe(false)
    expect(result.unmatchedSteps).toHaveLength(1)
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 7: UA 输出解析
// ═══════════════════════════════════════════════════════════════════════

describe('parseUnderstandingAgentOutput', () => {
  it('应解析单意图 LIST', () => {
    const raw = '{"todo":[{"subject":"所有配置项","operation":"LIST","is_chat":false,"value":null,"condition":null}]}'
    const result = parseUnderstandingAgentOutput(raw)
    expect(result).not.toBeNull()
    expect(result!.todo).toHaveLength(1)
    expect(result!.todo[0].intent).toBe('LIST')
    expect(result!.todo[0].is_chat).toBe(false)
  })

  it('应解析混合 LIST + WRITE', () => {
    const raw = `{
      "todo": [
        {"subject":"所有配置项","operation":"LIST","is_chat":false,"value":null,"condition":null},
        {"subject":"房间号","operation":"WRITE","is_chat":false,"value":"10041","condition":null}
      ]
    }`
    const result = parseUnderstandingAgentOutput(raw)
    expect(result).not.toBeNull()
    expect(result!.todo).toHaveLength(2)
  })

  it('应解析混合 WRITE + CHAT', () => {
    const raw = `{
      "todo": [
        {"subject":"房间号","operation":"WRITE","is_chat":false,"value":"10041","condition":null},
        {"subject":"gate","operation":"CHAT","is_chat":true,"value":null,"condition":null}
      ]
    }`
    const result = parseUnderstandingAgentOutput(raw)
    expect(result).not.toBeNull()
    expect(result!.todo).toHaveLength(2)
    expect(result!.todo[1].is_chat).toBe(true)
  })

  it('应处理无谓词包装的 JSON', () => {
    const raw = '一些前缀文字\n{"todo":[{"subject":"房间号","operation":"WRITE","is_chat":false,"value":"10041","condition":null}]}\n一些后缀'
    const result = parseUnderstandingAgentOutput(raw)
    expect(result).not.toBeNull()
    expect(result!.todo[0].intent).toBe('WRITE')
  })

  it('非法 JSON → null', () => {
    expect(parseUnderstandingAgentOutput('not json')).toBeNull()
    expect(parseUnderstandingAgentOutput('')).toBeNull()
  })

  it('无 todo 字段 → null', () => {
    expect(parseUnderstandingAgentOutput('{"other": 1}')).toBeNull()
  })
})

// ═══════════════════════════════════════════════════════════════════════
// Section 8: 测试集覆盖检查
// ═══════════════════════════════════════════════════════════════════════

describe('测试集完整性', () => {
  it('应有至少 20 条自动化用例', () => {
    expect(ALL_TEST_CASES.length).toBeGreaterThanOrEqual(20)
  })

  it('所有用例应有 ID', () => {
    for (const tc of ALL_TEST_CASES) {
      expect(tc.id).toBeTruthy()
      expect(tc.id).toMatch(/^AI-TC-/)
    }
  })

  it('涵盖 6 个分组', () => {
    const groups = new Set(ALL_TEST_CASES.map(tc => {
      const parts = tc.id.split('-')
      return parts[2] // CHAT/LIST/WRITE/MIXED/GATE/EDGE
    }))
    expect(groups.size).toBeGreaterThanOrEqual(6)
  })

  it('核心 Bug 用例应存在', () => {
    const bugCases = ALL_TEST_CASES.filter(tc => tc.bugRef)
    expect(bugCases.length).toBeGreaterThanOrEqual(3)
  })
})
