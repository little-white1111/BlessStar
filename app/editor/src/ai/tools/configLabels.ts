/**
 * configLabels — 配置字段 key → 中文标签映射，共享给所有配置相关 tool
 *
 * D38-5-INV-03: 数据由 BusinessAdapterRegistry 注入，核心仅保留框架。
 * 出厂时为空的 Record，启动时被业务适配器 populate。
 *
 * aiHint: LLM 模糊匹配时的完整自然语言描述（出厂基线不可删除，仅降权）
 */

import { BusinessAdapterRegistry } from '../business-adapter/registry'

// ── 运行时数据 ──────────────────────────────────────────────────────

export const KEY_LABELS: Record<string, string> = {}
export const AI_HINTS: Record<string, string> = {}
export const CONFIG_OPERATION_EXCEPTIONS: Record<string, string[]> = {}
const DEFAULT_OPERATIONS = ['READ', 'WRITE', 'VALIDATE', 'ADD_FIELD', 'SET_RULE', 'CREATE_RULE_CHAIN', 'VIEW_FILE'] as const
export const LABEL_TO_KEY: Record<string, string> = {}

/** 从 BusinessAdapterRegistry 同步业务数据 */
function syncFromRegistry(): void {
  const data = BusinessAdapterRegistry.getMergedAIData()
  if (data.configLabels) Object.assign(KEY_LABELS, data.configLabels)
  if (data.configDescriptions) Object.assign(AI_HINTS, data.configDescriptions)
  if (data.operationPermissions) Object.assign(CONFIG_OPERATION_EXCEPTIONS, data.operationPermissions)

  // 重建反向索引
  for (const [key, label] of Object.entries(KEY_LABELS)) {
    LABEL_TO_KEY[label] = key
  }
}

// 如果已有注册数据，立即同步
if (BusinessAdapterRegistry.initialized) {
  syncFromRegistry()
}

// ── 公开 API ────────────────────────────────────────────────────────

export function keyToLabel(key: string): string {
  return KEY_LABELS[key] || key.split('.').pop()?.replace(/_/g, ' ') || key
}

export function getAllowedOperations(configKey: string): string[] {
  const extras = CONFIG_OPERATION_EXCEPTIONS[configKey]
  if (!extras) return [...DEFAULT_OPERATIONS]
  return [...DEFAULT_OPERATIONS, ...extras]
}

export function validateOperation(configKey: string, operation: string): string | null {
  const allowed = getAllowedOperations(configKey)
  if (allowed.includes(operation)) return null
  const label = keyToLabel(configKey)
  return `"${label}" 不支持 ${operation} 操作，您可以 ${allowed.join('、')}`
}

/** 提供给外部调用，在注册适配器后同步数据 */
export function refreshFromRegistry(): void {
  syncFromRegistry()
}

/**
 * 运行时添加 label → configKey 别名映射。
 * 当用户说"将 X 作为 Y 的别名"时调用，后续 LABEL_TO_KEY 即可精确匹配。
 * @param alias 用户输入的别名（如 "房间"）
 * @param configKey 目标配置键（如 "livedesign.room.room_id"）
 */
export function addLabelMapping(alias: string, configKey: string): void {
  LABEL_TO_KEY[alias] = configKey
}
