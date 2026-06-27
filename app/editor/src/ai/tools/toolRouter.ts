/**
 * toolRouter — Tool Router meta-tool 模式
 *
 * 对应 GAP-09（只注入 1 个 meta-tool definition 到 context）。
 * 系统只暴露 `blessstar_tools` 一个工具，节省 87.5% tool definition token。
 *
 * 工作流程：
 * 1. AI 输出自然语言 intent → 系统收到
 * 2. Tool Router 通过 intent → tool mapping 路由到具体工具
 * 3. 匹配到的工具执行实际逻辑
 * 4. 未匹配时回退到 L2 Clean Format 降级
 *
 * 与 toolMatcher.ts 的区别：
 * - toolMatcher.ts 用于 L1 AdaptiveIndex 路径（索引命中时）
 * - toolRouter.ts 用于 Tool Router meta-tool 模式（AI 自然语言→系统路由）
 *
 * 两种模式通过开关切换，toolRouter.ts 只负责 intent→tool 的路由逻辑。
 */

import { matchTools, type ToolMatch } from '../tools/toolMatcher'
import { findTool } from '../executor'
import type { FunctionToolParam, ToolResult } from '../types'
import { FUNCTION_TOOLS } from '../tools'

// ── 元工具定义 ───────────────────────────────────────────────────────

/**
 * `blessstar_tools` — 唯一注入 AI context 的 tool definition。
 *
 * 代替原来的 13 个独立 tool definition，省 87.5% token。
 * AI 只需用自然语言描述意图，由 Tool Router 匹配到具体工具。
 */
export const META_TOOL_DEFINITION: FunctionToolParam = {
  name: 'blessstar_tools',
  description: `BlessStar 配置管理工具路由器。提供以下操作能力：
- 文件浏览：列出目录、读取文件、搜索内容、查找文件
- 配置管理：读写字段值、创建字段、校验配置
- 规则管理：更新 Gate 规则、生成归一化模板、推荐字段类型
- 系统诊断：读取诊断信息、终端命令执行（只读）
请用自然语言描述你想要的操作，系统会自动选择最合适的工具执行。`,
  parameters: {
    type: 'object',
    properties: {
      intent: {
        type: 'string',
        description: '用户操作意图的自然语言描述，如 "帮我看看 models 目录下有哪些文件"、"读取 room.json 的内容"',
      },
    },
    required: ['intent'],
  },
}

// ── 被屏蔽的独立 tool definitioins（由 META_TOOL_DEFINITION 替代） ──

let _metaMode = false

/**
 * 启用 meta-tool 模式：只返回 `blessstar_tools` 一个 definition。
 * 禁用时返回全部 13 个 tool definitions（回退兼容模式）。
 */
export function setMetaMode(enabled: boolean): void {
  _metaMode = enabled
}

/**
 * 是否处于 meta-tool 模式
 */
export function isMetaMode(): boolean {
  return _metaMode
}

/**
 * 获取注入 AI 的 tool definitions。
 * meta-tool 模式下只返回 1 个；
 * 传统模式返回全部 13 个。
 */
export function getInjectDefinitions(): FunctionToolParam[] {
  if (_metaMode) {
    return [META_TOOL_DEFINITION]
  }
  return FUNCTION_TOOLS.map((t) => t.definition)
}

// ── 意图路由 ─────────────────────────────────────────────────────────

export interface RouteResult {
  /** 是否成功路由到工具 */
  routed: boolean
  /** 匹配的工具名 */
  toolName?: string
  /** 从 intent 中提取的参数 */
  args: Record<string, unknown>
  /** 原始意图 */
  intent: string
  /** 未匹配时的降级消息 */
  fallbackMessage?: string
}

/**
 * 将自然语言 intent 路由到具体工具。
 *
 * 策略：
 * 1. 先用 AdaptiveIndex 精确匹配（L1）
 * 2. 若匹配到唯一工具，提取参数并路由
 * 3. 若匹配多个或未匹配，返回 fallback 由 LLM 降级处理
 */
export function routeIntent(intent: string): RouteResult {
  if (!intent || intent.trim().length === 0) {
    return {
      routed: false,
      args: {},
      intent,
      fallbackMessage: '意图不能为空，请描述你想要的操作',
    }
  }

  // 用 L1 AdaptiveIndex 匹配工具
  const matched: ToolMatch = matchTools(intent)

  if (matched.tools && matched.tools.length === 1) {
    // 精确匹配到一个工具
    const toolName = matched.tools[0]
    const tool = findTool(toolName)
    if (tool) {
      return {
        routed: true,
        toolName,
        args: extractArgsFromIntent(intent, tool.definition),
        intent,
      }
    }
  }

  if (matched.tools && matched.tools.length > 1) {
    // 匹配到多个工具，返回 Top-3 候选
    const candidates = matched.tools.slice(0, 3).join(', ')
    return {
      routed: false,
      args: {},
      intent,
      fallbackMessage: `匹配到多个工具: ${candidates}。请更具体地描述你的操作`,
    }
  }

  // 未匹配 → L2 Clean Format 降级
  return {
    routed: false,
    args: {},
    intent,
    fallbackMessage: matched.intent || '未能理解你的意图，请重新描述',
  }
}

/**
 * 从 intent 中提取工具参数（简单实现）。
 * 复杂场景：参数自动填充由 L1 AdaptiveIndex + Pre-Gate 完成。
 */
function extractArgsFromIntent(
  intent: string,
  definition: FunctionToolParam,
): Record<string, unknown> {
  const args: Record<string, unknown> = {}
  const props = definition.parameters?.properties as Record<string, { type: string; description: string }> | undefined
  if (!props) return args

  for (const [paramName, paramDef] of Object.entries(props)) {
    // 尝试从 intent 中提取参数值（简单启发式）
    if (paramDef.type === 'string') {
      // 查找可能的路径参数
      const pathMatch = intent.match(/([A-Za-z]:\\[^\s,，。]*)/)
      if (pathMatch && paramName === 'path') {
        args[paramName] = pathMatch[1]
      }
      // 查找关键词模式参数
      const keyMatch = intent.match(/(?:查找|搜索|找|查)[的]?\s*"?([^"\s,，。]+)"?/)
      if (keyMatch && paramName === 'pattern') {
        args[paramName] = keyMatch[1]
      }
    }
  }

  return args
}

/**
 * 执行路由后的工具调用。
 * 与 executor.ts 的 executeToolCall 类似，但走 Tool Router 模式。
 */
export async function executeRoutedTool(
  toolName: string,
  args: Record<string, unknown>,
): Promise<ToolResult> {
  const tool = findTool(toolName)
  if (!tool) {
    return { success: false, error: `未知工具: ${toolName}` }
  }

  return tool.execute(args)
}
