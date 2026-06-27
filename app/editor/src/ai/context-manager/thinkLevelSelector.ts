/**
 * thinkLevelSelector — 推理强度自动选择器
 *
 * 对应 GAP-14（推理强度自动选择——按意图复杂度分配计算预算）。
 * 参考 DeepSeek V4 的三档推理模式：Non-think / Think High / Think Max。
 *
 * BlessStar 的四层路由管线映射到三档推理强度：
 *
 * | 路由层 | 等价推理模式 | 计算预算 | 覆盖预期 |
 * |--------|-------------|---------|---------|
 * | L0 Skill Router | Non-think（0 LLM） | 0 token | 40-60% |
 * | L1 AdaptiveIndex | Think Low（索引查询） | <64K | 25-35% |
 * | L2 Clean Format | Think High（受限 LLM） | 128K | 10-15% |
 * | L3 JSON 回退 | Think Max（全强度 LLM） | ≥384K | 1-5% |
 */

import { matchSkill, parseCommand } from './skillRouter'
import { matchTools } from '../tools/toolMatcher'

// ── 推理强度枚举 ─────────────────────────────────────────────────────

export type ThinkLevel = 'non_think' | 'think_low' | 'think_high'

export interface ThinkLevelConfig {
  level: ThinkLevel
  /** 对应 V4 的推理模式描述 */
  v4Analogy: string
  /** 建议的 token 预算 */
  suggestedTokenBudget: number
  /** 建议的 temperature */
  suggestedTemperature: number
  /** 是否需要 LLM 调用 */
  requiresLLM: boolean
  /** 是否需要上下文注入 */
  requiresContextInjection: boolean
}

const THINK_LEVEL_CONFIGS: Record<ThinkLevel, ThinkLevelConfig> = {
  non_think: {
    level: 'non_think',
    v4Analogy: 'Non-think — 无需 LLM 调用',
    suggestedTokenBudget: 0,
    suggestedTemperature: 0,
    requiresLLM: false,
    requiresContextInjection: false,
  },
  think_low: {
    level: 'think_low',
    v4Analogy: 'Think Low — 仅索引查询，无需 LLM 推理',
    suggestedTokenBudget: 64 * 1024,
    suggestedTemperature: 0,
    requiresLLM: false,
    requiresContextInjection: true,
  },
  think_high: {
    level: 'think_high',
    v4Analogy: 'Think High — 全强度 LLM 推理',
    suggestedTokenBudget: 128 * 1024,
    suggestedTemperature: 0.3,
    requiresLLM: true,
    requiresContextInjection: true,
  },
}

// ── 路由上下文 ───────────────────────────────────────────────────────

export interface RoutingContext {
  userInput: string
  /** 是否启用了 Skill Router */
  skillRouterEnabled: boolean
  /** 是否启用了 Tool Router meta-tool 模式 */
  metaModeEnabled: boolean
  /** 历史意图命中率（用于自适应降级） */
  historyHitRate?: number
}

// ── 自动选择 ─────────────────────────────────────────────────────────

/**
 * 根据用户输入和路由上下文自动选择推理强度。
 *
 * 选择策略：
 * 1. L0 Skill Router 命中 → non_think（0 LLM）
 * 2. L1 AdaptiveIndex 命中且参数可自动填充 → think_low（索引查询）
 * 3. 其余场景 → think_high（降级到 LLM）
 */
export function selectThinkLevel(
  intent: string,
  context: RoutingContext,
): ThinkLevelConfig {
  // L0: Skill Router / UNIFIED_SKILLS 命中 → non_think
  if (context.skillRouterEnabled) {
    if (matchSkill(intent).matched) return THINK_LEVEL_CONFIGS.non_think
    if (parseCommand(intent).matched) return THINK_LEVEL_CONFIGS.non_think
  }

  // L1: AdaptiveIndex 精确匹配且可自动填充 → think_low
  const matched = matchTools(intent)
  if (matched.tools && matched.tools.length > 0) {
    // 检查是否所有必填参数都可自动填充
    const allParamsAutoFillable = checkParamsAutoFillable(matched.tools[0], intent)
    if (matched.tools.length === 1 && allParamsAutoFillable) {
      return THINK_LEVEL_CONFIGS.think_low
    }
  }

  // 降级 → think_high
  return THINK_LEVEL_CONFIGS.think_high
}

/**
 * 获取指定推理强度的配置
 */
export function getThinkLevelConfig(level: ThinkLevel): ThinkLevelConfig {
  return THINK_LEVEL_CONFIGS[level]
}

// ── 辅助函数 ─────────────────────────────────────────────────────────

/**
 * 检查工具的必填参数是否可以从意图中自动提取（天真实现）。
 * 用于决定是否可以走 think_low 路径（不经过 LLM 参数生成）。
 */
function checkParamsAutoFillable(toolName: string, intent: string): boolean {
  // 如果 intent 中包含路径、字段名等结构化信息，认为可自动填充
  const hasPath = /[A-Za-z]:\\\\/.test(intent) || /[\\/][\w.-]+[\\/]/.test(intent)
  const hasKey = /['"][\w.]+['"]/.test(intent)
  const hasPattern = /[*?]/.test(intent)

  switch (toolName) {
    case 'list_directory':
    case 'read_file':
      return hasPath
    case 'find_files':
      return hasPath || hasPattern
    case 'search_content':
      return intent.length > 5 // 随意搜索词
    case 'read_config_value':
    case 'write_config_value':
      return hasKey
    default:
      return false // 其他工具默认需 LLM 推理参数
  }
}
