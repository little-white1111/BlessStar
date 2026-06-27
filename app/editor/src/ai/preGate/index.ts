/**
 * preGate — 工具入参前置校验系统
 *
 * 对应缺口四（只校验输出，不校验输入）的 BlessStar-native 方案。
 * 每个工具可注册 Pre-Gate 规则（声明式 JSON），executor 在执行前
 * 调用此模块统一评估。不走 per-tool 函数代码，零额外维护。
 *
 * DAY38-06: 二段式校验
 *   Stage 1: JS 快筛 (not_empty/regex_match/regex_not_match)
 *   Stage 2: C 深判 (bs_gate_evaluator_evaluate)
 */

import type { PreGateRule, PreGateRulesMap, ToolResult } from '../types'

// ── 内置校验函数（Stage 1: JS 快筛）───────────────────────────────────

type Evaluator = (value: unknown, rule: PreGateRule) => string | null // null=通过, string=错误消息

const builtinEvaluators: Record<string, Evaluator> = {
  not_empty(value, rule) {
    if (value === undefined || value === null || value === '') {
      return rule.error
    }
    return null
  },
  regex_match(value, rule) {
    if (typeof value !== 'string' || !rule.pattern) return rule.error
    try {
      return new RegExp(rule.pattern).test(value) ? null : rule.error
    } catch {
      return `正则解析失败: ${rule.pattern}`
    }
  },
  regex_not_match(value, rule) {
    if (typeof value !== 'string' || !rule.pattern) return null
    try {
      return new RegExp(rule.pattern).test(value) ? rule.error : null
    } catch {
      return null
    }
  },
}

// ── 自定义校验函数注册中心 ─────────────────────────────────────────────

const customEvaluators = new Map<string, Evaluator>()

export function registerCustomEvaluator(name: string, fn: Evaluator): void {
  customEvaluators.set(name, fn)
}

// ── 核心 evaluate ─────────────────────────────────────────────────────

function getEvaluator(rule: PreGateRule): Evaluator | undefined {
  if (rule.type === 'custom' && rule.evaluator) {
    return customEvaluators.get(rule.evaluator)
  }
  return builtinEvaluators[rule.type]
}

/**
 * 评估一组参数通过 Pre-Gate 规则（Stage 1: JS 快筛）。
 * 返回 null 表示全部通过；返回字符串表示第一条失败的错误消息。
 */
export function evaluatePreGates(
  rules: PreGateRule[] | undefined,
  args: Record<string, unknown>,
): string | null {
  if (!rules || rules.length === 0) return null

  for (const rule of rules) {
    const evaluator = getEvaluator(rule)
    if (!evaluator) continue

    const error = evaluator(args[rule.field], rule)
    if (error !== null) {
      return error
    }
  }

  return null
}

/**
 * Stage 2: C 侧深判 — 调用 bs_gate_evaluator_evaluate 进行语义级校验。
 * 通过 IPC → native → C ABI 链路执行。
 * 返回 null 表示通过；返回字符串表示错误消息。
 */
export async function deepEvaluate(
  toolName: string,
  args: Record<string, unknown>,
): Promise<string | null> {
  try {
    // 为每个参数构建 C 侧 gate 评估
    for (const [key, value] of Object.entries(args)) {
      const chainJson = JSON.stringify({
        version: '1.0',
        root: {
          type: 'bs_condition',
          id: `deep_${toolName}_${key}`,
          field_key: key,
          op: 'exists',
          value: 'true',
        },
      })

      const valueStr = String(value ?? '')

      const resultJson = await (window as any).blessstar.executeTool({
        tool: 'gate_evaluator',
        args: {
          chain_json: chainJson,
          field_key: key,
          field_value: valueStr,
        },
      })

      const result = typeof resultJson === 'string' ? JSON.parse(resultJson) : resultJson
      if (result && !result.passed) {
        return `[C-深判] 字段 ${key}: ${result.error_message || '校验失败'}`
      }
    }
    return null
  } catch (err) {
    // C 侧不可用时降级为通过（不阻塞流程）
    console.warn('[deepEvaluate] C 侧调用失败，降级为通过:', err)
    return null
  }
}

/**
 * Pre-Gate 规则集合注册表（按工具名索引）
 *
 * 各工具在各自的文件中导出一个 `preGateRules` 常量，
 * 在此处统一注册（未来可改为 JSON 文件按需加载）。
 */
export const TOOL_PRE_GATE_RULES: PreGateRulesMap = {}
