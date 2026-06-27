/* GateFactoryBridge — TypeScript-side integration for Gate factory operations.
 * Maps TS calls to existing IPC tool executeTool / validateConfig / update_gate_rule. */

import type { ToolResult } from './types'
import { validateGateRule, formatValidationErrors } from './validator'

/* ── Types (mirroring C-layer bs_gate_rule_def_t, bs_gate_factory_t) ── */

export type GateLayer = 'default' | 'policy' | 'custom'

export interface GateRuleDef {
  field_key: string
  field_type?: string
  op?: string
  value?: string
  scenario?: string
  layer?: GateLayer
  ai_hint?: string
}

export interface GateFactory {
  name: string
  layer: GateLayer
}

export interface GateMatchRequest {
  field_key: string
  field_type: string
  scenario?: string
}

export interface GateMatchResult {
  nodes: Array<{
    id: string
    type: string
    field_key: string
    op: string
    value: string
    layer: number
    sub_category: string
    domain: string
    entity: string
  }>
  node_count: number
  confidence: number
}

export interface GateEvalContext {
  field_key: string
  field_value: string
}

export interface GateEvalResult {
  passed: boolean
  failed_layer: number
  failed_node_index: number
  error_message: string | null
}

/* ── Factory singletons ─────────────────────────────────────────────── */

export const GATE_FACTORIES: GateFactory[] = [
  { name: 'default', layer: 'default' },
  { name: 'policy', layer: 'policy' },
  { name: 'custom', layer: 'custom' },
]

/** Lookup factory by name */
export function gateFactoryByName(name: string): GateFactory | undefined {
  return GATE_FACTORIES.find((f) => f.name === name)
}

/** Lookup factory by layer */
export function gateFactoryByLayer(layer: GateLayer): GateFactory | undefined {
  return GATE_FACTORIES.find((f) => f.layer === layer)
}

/* ── Infer sub_category from rule semantics (mirrors C logic) ──────── */

const THRESHOLD_KEYWORDS = ['阈值', 'threshold', '限制', 'limit', '上限', '下限', 'max', 'min']
const APPROVAL_KEYWORDS = ['审批', 'approve', 'approval', '审核']
const ALERT_KEYWORDS = ['告警', 'alert', 'warn', '提醒']
const ENUM_CHECK_KEYWORDS = ['枚举', 'enum', '选项']
const FORMAT_KEYWORDS = ['格式', 'format', '模式', 'pattern']

function keywordMatch(text: string, keywords: string[]): boolean {
  const lower = text.toLowerCase()
  return keywords.some((k) => lower.includes(k))
}

export function inferSubCategory(rule: GateRuleDef): string {
  const op = rule.op || ''
  const hint = rule.ai_hint || ''

  if (['gt', 'lt', 'gte', 'lte', 'range'].includes(op)) return 'threshold'
  if (['eq', 'ne'].includes(op) && keywordMatch(hint, ENUM_CHECK_KEYWORDS)) return 'enum_check'
  if (['eq', 'ne'].includes(op) && keywordMatch(hint, FORMAT_KEYWORDS)) return 'format'
  if (['eq', 'ne'].includes(op)) return 'format'
  if (['in'].includes(op)) return 'enum_check'
  if (['match'].includes(op)) return 'format'

  if (keywordMatch(hint, THRESHOLD_KEYWORDS)) return 'threshold'
  if (keywordMatch(hint, APPROVAL_KEYWORDS)) return 'approval'
  if (keywordMatch(hint, ALERT_KEYWORDS)) return 'alert'
  if (keywordMatch(hint, ENUM_CHECK_KEYWORDS)) return 'enum_check'
  if (keywordMatch(hint, FORMAT_KEYWORDS)) return 'format'

  return 'threshold'
}

/* ── Build stable_key from rule (mirrors C logic) ──────────────────── */

export function buildStableKey(rule: GateRuleDef): string {
  const scenario = rule.scenario || 'default'
  const fk = rule.field_key || '*'
  const layer = rule.layer || 'default'
  const layerMap: Record<string, string> = { default: '0', policy: '1', custom: '2' }
  const sub = inferSubCategory(rule)
  return `${scenario}:${fk}:${layerMap[layer]}:${sub}`
}

/* ── Produce a gate via factory (delegates to IPC tool) ────────────── */

export async function gateFactoryProduce(
  factory: GateFactory,
  rule: GateRuleDef,
): Promise<ToolResult> {
  const stableKey = buildStableKey(rule)
  const subCategory = inferSubCategory(rule)

  const gateJson = {
    type: factory.layer === 'default' ? 'bs_condition'
      : factory.layer === 'policy' ? 'bs_policy_attr'
      : 'bs_custom_gate',
    id: stableKey,
    field_key: rule.field_key,
    op: rule.op || 'eq',
    value: rule.value || '',
    layer: factory.layer === 'default' ? 0 : factory.layer === 'policy' ? 1 : 2,
    stable_key: stableKey,
    sub_category: subCategory,
    domain: rule.scenario || 'default',
    entity: rule.field_key || '*',
  }

  // Validate before producing
  const validation = await validateGateRule(JSON.stringify({
    gate_id: stableKey,
    scenario: rule.scenario || 'production',
    do: [{ type: gateJson.type, field: rule.field_key, operator: rule.op || 'eq', value: rule.value || '' }],
  }))

  if (!validation.valid) {
    return {
      success: false,
      error: `Gate 工厂产出校验失败:\n${formatValidationErrors(validation)}`,
    }
  }

  return {
    success: true,
    data: gateJson,
  }
}

/* ── Match gates by field + scenario ────────────────────────────────── */

export async function gateMatcherMatch(
  _request: GateMatchRequest,
): Promise<GateMatchResult> {
  // For MVP: returns placeholder
  // Full impl would query a loaded gate registry via IPC
  return {
    nodes: [],
    node_count: 0,
    confidence: 0.0,
  }
}

/* ── Evaluate a single field value against gate chain ──────────────── */

export async function gateEvaluatorEvaluate(
  _ctx: GateEvalContext,
): Promise<GateEvalResult> {
  // For MVP: always pass
  // Full impl would call bs_gate_evaluator_evaluate via napi-rs
  return {
    passed: true,
    failed_layer: 0,
    failed_node_index: 0,
    error_message: null,
  }
}
