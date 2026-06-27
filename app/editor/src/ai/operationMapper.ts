/**
 * 判断 intent 是否与 valueType 兼容。
 * D38-10-FIX: QUERY_LIST 只兼容 directory/file/file_path（→BROWSE_DIR）和 无key（→LIST）；
 *             QUERY_SINGLE 只兼容 single-value 类型；QUERY_ENUM 只兼容 enum 类型。
 * 用于候选列表过滤，确保不把 QUERY_SINGLE 的候选误推到 QUERY_LIST 的 ASK 列表中。
 *
 * @param intent UA intent
 * @param valueType 字段值类型（optional，无 valueType 视为兼容）
 * @returns 是否兼容
 */
export function isIntentCompatibleWithValueType(intent?: string, valueType?: string): boolean {
  if (!intent) return true
  if (!valueType) return true

  // QUERY_LIST: 只兼容 directory（列目录下文件）或无 valueType（LIST全部配置）
  // file/file_path 是单个文件/URL，不可被"列出"（应 READ/VIEW_FILE）
  if (intent === 'QUERY_LIST') {
    return valueType === 'directory'
  }

  // QUERY_SINGLE: 只兼容 single-value 类型（READ操作）
  if (intent === 'QUERY_SINGLE') {
    return valueType !== 'directory' && valueType !== 'file' && valueType !== 'file_path'
  }

  // QUERY_ENUM: 兼容所有类型（LIST枚举值）
  if (intent === 'QUERY_ENUM') return true

  // MODIFY/ACTION: 不限制
  return true
}

/**
 * operationMapper — Tool ↔ Operation ↔ Intent 确定性映射层（专题七：3意图精简版）
 *
 * D38-7-INV-05: 确定性路由层接管 8 种操作区分
 * D38-7-INV-04: UA 意图 11→3 — QUERY / MODIFY / ACTION
 *
 * 三层映射：
 *   UA 产出 3 种 intent → 路由层按 value_type/semantic_group 决策 → operation → tool 链
 *
 * 旧版 LOOKUP/RULE/BROWSE/SEARCH_FIND 等意图不再由 UA 产出，
 * 其操作由路由层根据 value_type 和 semantic_group 自动决策。
 */

import { validateOperation as validateConfigOp } from './tools/configLabels'

// ── Tool → Operation 反向映射 ────────────────────────────────────────

export const TOOL_TO_OPERATION: Record<string, string> = {
  read_config_value:              'READ',
  write_config_value:             'WRITE',
  list_configs:                   'LIST',
  validate_config:                'VALIDATE',
  create_schema_field:            'ADD_FIELD',
  update_gate_rule:               'SET_RULE',
  create_gate_chain:              'CREATE_RULE_CHAIN',
  list_directory:                 'BROWSE',
  search_content:                 'SEARCH',
  find_files:                     'FIND',
  read_file:                      'VIEW_FILE',
  run_terminal:                   'EXEC',
  read_diagnostics:               'DIAGNOSE',
  generate_normalizer_template:   'GENERATE',
  chat:                           'CHAT',
  execute_query:                  'QUERY',
  ask_user:                       'ASK',
}

// ── Operation → Tool 正向映射（完整 16 种 → tool 名称链）──────────────

const OPERATION_TO_TOOLS: Record<string, string[]> = {
  'READ':              ['read_config_value'],
  'WRITE':             ['read_config_value', 'write_config_value'],
  'LIST':              ['list_configs'],
  'VALIDATE':          ['validate_config'],
  'ADD_FIELD':         ['create_schema_field'],
  'SET_RULE':          ['update_gate_rule'],
  'CREATE_RULE_CHAIN': ['create_gate_chain'],
  'BROWSE':            ['read_config_value', 'list_directory'],
  'BROWSE_DIR':        ['list_directory'],
  'SEARCH':            ['search_content'],
  'FIND':              ['find_files'],
  'VIEW_FILE':         ['read_file'],
  'EXEC':              ['run_terminal'],
  'DIAGNOSE':          ['read_diagnostics'],
  'GENERATE':          ['generate_normalizer_template'],
  'CHAT':              ['chat'],
  'QUERY':             ['write_config_value', 'execute_query', 'read_config_value'],
  'ASK':               ['ask_user'],
}

// ── 专题七：3 意图 → 操作路由 ─────────────────────────────────────

/**
 * INTENT_TO_OPERATION（专题七精简版）：3 意图映射。
 *
 * 旧版 11 意图（LOOKUP/RULE/BROWSE/SEARCH_FIND/EXEC/GENERATE 等）不再由 UA 产出。
 * QUERY/MODIFY 的具体操作由 value_type/semantic_group 决策函数进一步细化。
 * ACTION 的操作由 actionSubType 决策函数细化。
 */
export const INTENT_TO_OPERATION: Record<string, string[]> = {
  'QUERY':        ['READ', 'LIST'],      // 由路由层按 value_type 决策（旧版兼容）
  'QUERY_SINGLE': ['READ'],              // D38-9-INV-02: 具体值查询
  'QUERY_LIST':   ['LIST'],              // D38-9-INV-02: 列表查询
  'QUERY_ENUM':   ['LIST'],              // D38-9-INV-02: 枚举范围查询
  'MODIFY':       ['READ', 'WRITE'],     // 由路由层按 semantic_group 决策
  'ACTION':       ['CHAT', 'EXEC', 'GENERATE'],  // 由路由层按 actionSubType 决策
}

// ── Semantic Group 推断 ──────────────────────────────────────────

/**
 * Semantic Group：从 configKey 推断配置项的语义分组。
 * 用于路由层决策操作类型。
 *
 * - 'config': 普通配置项 → read/write pair
 * - 'rule': Gate 规则相关 → update_gate_rule / create_gate_chain
 * - 'schema': Schema 字段相关 → create_schema_field
 */
export type SemanticGroup = 'config' | 'rule' | 'schema'

/**
 * 从 configKey 推断语义分组。
 * 规则关键词默认归为 'rule'，schema 相关归为 'schema'，其余为 'config'。
 */
export function inferSemanticGroup(configKey: string): SemanticGroup {
  if (!configKey) return 'config'
  const lower = configKey.toLowerCase()
  if (lower.includes('gate') || lower.includes('rule') || lower.includes('规则') || lower.includes('chain')) {
    return 'rule'
  }
  if (lower.includes('schema') || lower.includes('field') || lower.includes('字段')) {
    return 'schema'
  }
  return 'config'
}

// ── Action SubType 推断 ──────────────────────────────────────────

/**
 * ACTION 意图的子类型。
 * - 'chat': 纯概念咨询（is_chat=true）
 * - 'exec': 执行命令
 * - 'generate': 生成模板
 */
export type ActionSubType = 'chat' | 'exec' | 'generate'

/**
 * 从用户输入推断 ACTION 子类型。
 */
export function inferActionSubType(userInput: string): ActionSubType {
  if (!userInput) return 'chat'
  const lower = userInput.toLowerCase()
  if (/执行|运行|tree|dir|ls|pwd/.test(lower)) return 'exec'
  if (/生成.*模板|归一化|厂商映射/.test(lower)) return 'generate'
  return 'chat'
}

// ── 路由层决策函数 ──────────────────────────────────────────────

/**
 * 根据 intent 子类型 + value_type + groupHint 决定 QUERY 意图的具体操作。
 *
 * D38-9-INV-04: groupHint 优先级高于 valueType
 * - QUERY_SINGLE → READ（具体值查询，即使是 file 类型也读值）
 * - QUERY_LIST → LIST 或 BROWSE_DIR（列表查询）
 * - QUERY_ENUM → LIST（枚举范围查询）
 * - 旧版 QUERY → 按 value_type 回退逻辑
 *
 * @param intent 意图（QUERY_SINGLE | QUERY_LIST | QUERY_ENUM | QUERY）
 * @param valueType 字段类型
 * @param hasConfigKey 是否有候选配置键
 * @returns 操作名
 */
export function routeQueryByValueType(
  intent?: string,
  valueType?: string,
  hasConfigKey?: boolean,
): string {
  // D38-9-INV-02: 按子意图直接路由（绕过 value_type 启发式）
  if (intent === 'QUERY_SINGLE') {
    return 'READ'  // 具体值查询，即使是 file 类型
  }
  if (intent === 'QUERY_LIST') {
    // directory → 列出目录下文件；有具体 key 的非目录类型 → 实际是查单个值
    if (valueType === 'directory') return 'BROWSE_DIR'
    if (hasConfigKey) return 'READ'
    return 'LIST'
  }
  if (intent === 'QUERY_ENUM') {
    return 'LIST'  // 枚举范围也走列表查询
  }

  // 旧版 QUERY 回退逻辑
  if (!hasConfigKey) return 'LIST'
  if (valueType === 'file') return 'BROWSE_DIR'
  return 'READ'
}

/**
 * 根据 semantic_group 决定 MODIFY 意图的具体操作链。
 *
 * - 'rule' → SET_RULE（更新 Gate 规则）
 * - 'schema' → ADD_FIELD（新增 Schema 字段）
 * - 'config' → READ + WRITE（标准读写对）
 */
export function routeModifyBySemanticGroup(semanticGroup: SemanticGroup): string[] {
  switch (semanticGroup) {
    case 'rule':
      return ['SET_RULE']
    case 'schema':
      return ['ADD_FIELD']
    case 'config':
    default:
      return ['READ', 'WRITE']
  }
}

/**
 * 根据 actionSubType 决定 ACTION 意图的具体操作。
 */
export function routeActionBySubType(subType: ActionSubType): string[] {
  switch (subType) {
    case 'exec':
      return ['EXEC']
    case 'generate':
      return ['GENERATE']
    case 'chat':
    default:
      return ['CHAT']
  }
}

/**
 * 核心路由函数：将 3 意图 + target_config_key + value_type 映射为操作列表。
 *
 * D38-7-INV-05: value_type / semantic_group 决策
 *
 * @param intent UA 输出的 3 意图之一
 * @param targetConfigKey 匹配的配置键（可能为空）
 * @param valueType 字段类型（从检索层获取）
 * @param userInput 用户原始输入（用于 ACTION 子类型推断）
 * @returns 操作列表（按执行顺序）
 */
export function routeIntentToOperations(
  intent: string,
  targetConfigKey?: string | null,
  valueType?: string,
  userInput?: string,
): string[] {
  const hasKey = !!targetConfigKey

  switch (intent) {
    case 'QUERY':
    case 'QUERY_SINGLE':
    case 'QUERY_LIST':
    case 'QUERY_ENUM': {
      const op = routeQueryByValueType(intent, valueType, hasKey)
      return [op]
    }
    case 'MODIFY': {
      if (!hasKey) return ['READ', 'WRITE']  // 默认 fallback
      const semanticGroup = inferSemanticGroup(targetConfigKey!)
      return routeModifyBySemanticGroup(semanticGroup)
    }
    case 'ACTION': {
      const subType = inferActionSubType(userInput || '')
      return routeActionBySubType(subType)
    }
    default:
      return ['READ']
  }
}

// ── 系统级操作（不绑定 configKey）────────────────────────────────

export const SYSTEM_SCOPED_OPS = new Set([
  'LIST', 'BROWSE', 'BROWSE_DIR', 'SEARCH', 'FIND', 'EXEC', 'DIAGNOSE', 'GENERATE', 'CHAT', 'QUERY', 'ASK',
])

// ── 配置级操作（需要 configKey 校验）────────────────────────────────

export const CONFIG_SCOPED_OPS = new Set([
  'READ', 'WRITE', 'VALIDATE', 'ADD_FIELD', 'SET_RULE', 'CREATE_RULE_CHAIN', 'VIEW_FILE',
])

// ── Executor 级操作（需要 executor pattern 匹配）───────────────────

export const EXECUTOR_SCOPED_OPS = new Set([
  'QUERY',
])

// ── LOOKUP 解析结果（旧版，保留类型兼容；resolveLookupIntent 已删除）───

/** @deprecated 专题七后 LOOKUP 已由检索层+路由表替代 */
export type LookupResult =
  | { route: 'configKey'; configKey: string; operation: 'READ' }
  | { route: 'executor'; pattern: string; operation: 'QUERY' }
  | { route: 'diagnostics'; operation: 'DIAGNOSE' }
  | { route: 'file'; configKey: string; operation: 'VIEW_FILE' }
  | { route: 'unresolved'; candidates: string[]; originalSubject: string }

// ── 核心函数 ──────────────────────────────────────────────────────────

/**
 * 将 operation 枚举值转为 tool 名称数组（按执行顺序）。
 */
export function operationToTools(operation: string): string[] {
  return OPERATION_TO_TOOLS[operation] || ['read_config_value']
}

/**
 * 根据 intent 获取可能对应的操作列表。
 *
 * 专题九：接受 6 种 intent（QUERY/QUERY_SINGLE/QUERY_LIST/QUERY_ENUM/MODIFY/ACTION），
 * 旧版 11 种 intent 兼容降级（返回空数组，交由路由层处理）。
 */
export function intentToOperations(intent: string): string[] {
  // 映射新意图
  if (INTENT_TO_OPERATION[intent]) {
    return INTENT_TO_OPERATION[intent]
  }
  // 旧版 intent 兼容：返回空数组，由调用方 fallback
  return []
}

/**
 * 校验指定 operation 对 configKey 是否合法。
 */
export function validateOperationForConfig(configKey: string, operation: string): string | null {
  if (SYSTEM_SCOPED_OPS.has(operation) || EXECUTOR_SCOPED_OPS.has(operation)) return null
  return validateConfigOp(configKey, operation)
}
