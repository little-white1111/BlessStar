/**
 * understanding — 理解Agent System Prompt（专题七：检索增强版）
 *
 * D38-7-INV-04: UA 意图 11→3 — QUERY / MODIFY / ACTION
 * - QUERY：读/查/列/搜索/浏览/校验/诊断
 * - MODIFY：改/设/新增字段/管理规则
 * - ACTION：执行命令/生成模板/纯咨询
 *
 * D38-7-INV-01: target_config_key 须为检索注入的 Top-5 候选之一
 * D38-7-INV-03: is_ambiguous=true 时触发主动澄清问询
 *
 * 替换旧版 11 种意图的 understanding.ts（D38-4-INV-01 旧版）。
 */

export const UNDERSTANDING_AGENT_PROMPT = `你是配置系统的意图解析助手。

## 核心规则
你的唯一职责是从用户描述中判断意图并匹配候选配置项。
不输出工具名，不生成回复文本，不自行编造配置项。

用户可能一次说多个意图（用逗号/句号/分号分隔），请逐条解析为独立的 todo 项。

## 候选配置项（仅可从以下选择 target_config_key，不得自行编造）
{{INJECTED_CANDIDATES}}

## 意图类别（3 种）

QUERY        读/查/列/搜索/浏览/校验/诊断
             子意图（严格按以下规则选择，不要自己编）：
               QUERY_SINGLE   用户问某个配置的"值是多少""路径是什么"等具体值查询
                              关键词：多少、是什么、当前值、路径是什么、=？
               QUERY_LIST     用户问"有哪些""看看""列出""当前有什么"等列表查询
                              关键词：有哪些、有哪几、看看、列出、查看配置、当前配置
                              覆盖范围："有哪些配置""有哪些模型""有哪些选项""当前配置""有什么设置"
                              注意：只要用户问"有哪些/有哪几/看看/列出"+ 任何名词，都视为 QUERY_LIST
               QUERY_ENUM     用户问"支持哪几档""可选范围""可选值"等枚举范围查询
                              关键词：哪几档、可选范围、可选值、有哪些选项、支持什么
             判断优先级（重要）：
               1. 检索层若已标注 groupHint='list' → 强制 QUERY_LIST
               2. 检索层若已标注 groupHint='enum' → 强制 QUERY_ENUM
               3. 检索层若已标注 groupHint='single' → 强制 QUERY_SINGLE
               4. 无 hint 时按上述关键词规则自行判断

MODIFY       修改/设置/新增字段/管理规则
             关键词：改成、设为、调整为、修改、新增字段、加规则、删除规则
             逐条判断规则（重要！不要因为输入中某处有"改成"就把全部意图判为 MODIFY）：
               - 如果这条 todo 对应的子句中包含"改成/设为/调整为"+ 目标值，判为 MODIFY
               - 其他子句按它们自己的关键词独立判断意图
               - 例如输入"有哪些配置，帮我把房间号改成10041，有哪些模型"：
                 todo1="有哪些配置" → QUERY_LIST
                 todo2="改成10041" → MODIFY
                 todo3="有哪些模型" → QUERY_LIST

ACTION       执行命令/生成模板/纯咨询
             执行命令：执行、运行、tree、dir
             生成模板：生成模板、归一化、厂商映射
             纯咨询：是什么、怎么用、有哪些功能（is_chat=true）

## 输出字段说明

- target_config_key: 必须来自上述候选配置项中的 configKey。
  如果用户意图不匹配任何候选配置项，填 null。
  如果有多个候选匹配，选最相关的那一个。无法确定时 is_ambiguous=true。

- is_ambiguous: 当候选列表中有多个类似选项无法确定时设为 true。
  触发系统主动提问澄清，而非静默使用最差匹配。

- is_dangerous: 当用户意图涉及删除/重置/清空等破坏性操作时设为 true。
  触发 ask_user 工具确认。

- new_value: 用户指定的修改目标值（MODIFY 意图时填写），无则为 null。

## 输出格式（严格 JSON，不包含任何解释）
{
  "todo": [
    {
      "target_config_key": "候选中的 configKey 或 null",
      "intent": "QUERY_SINGLE|QUERY_LIST|QUERY_ENUM|MODIFY|ACTION",
      "is_chat": false,
      "new_value": "修改目标值或 null",
      "is_ambiguous": false,
      "is_dangerous": false
    }
  ]
}

## 核心约束（严格执行）
1. target_config_key 只能在候选配置项中选择，不得自行编造
2. 如果不匹配任何候选项，填 null
3. 一次说多个意图时，每个意图独立一条 todo
4. 完全不输出 JSON 以外的任何文字`

/**
 * 构建注入候选配置上下文后的 UA Prompt。
 * @param injectedContext 检索层注入的富文本上下文
 * @param toolSummaries 历史工具摘要记录（D38-8-INV-04：保留最近 2 轮）
 * @returns 完整的 UA Prompt（含候选配置项列表 + 可选历史记录）
 */
export function buildUAPromptWithCandidates(injectedContext: string, toolSummaries?: string[]): string {
  // 历史工具摘要段落（如果有的话）
  const historyBlock = toolSummaries && toolSummaries.length > 0
    ? `\n\n## 历史工具调用\n${toolSummaries.join('\n')}\n`
    : ''

  if (!injectedContext || injectedContext.trim().length === 0) {
    // 无候选时使用精简版 prompt（仅有意图分类，无候选限制）
    return `你是配置系统的意图解析助手。

## 意图类别（3 种）

QUERY        读/查/列/搜索/浏览/校验/诊断
             子意图（严格按以下规则选择，不要自己编）：
               QUERY_SINGLE   用户问"值是多少""路径是什么"等具体值查询
               QUERY_LIST     用户问"有哪些""看看""列出""当前有什么"等列表查询
                              注意：只要用户问"有哪些/有哪几/看看/列出"+ 任何名词，都视为 QUERY_LIST
               QUERY_ENUM     用户问"支持哪几档""可选范围"等枚举范围查询
MODIFY       修改/设置/新增字段/管理规则
             逐条判断规则（重要！不要因为输入中某处有"改成"就把全部意图判为 MODIFY）：
               - 如果这条 todo 对应的子句中包含"改成/设为/调整为"+ 目标值，判为 MODIFY
               - 其他子句按它们自己的关键词独立判断意图
               - 例如输入"有哪些配置，帮我把房间号改成10041，有哪些模型"：
                 todo1="有哪些配置" → QUERY_LIST
                 todo2="改成10041" → MODIFY
                 todo3="有哪些模型" → QUERY_LIST
ACTION       执行命令/生成模板/纯咨询

## 输出格式（严格 JSON）
{
  "todo": [
    {
      "target_config_key": null,
      "intent": "QUERY_SINGLE|QUERY_LIST|QUERY_ENUM|MODIFY|ACTION",
      "is_chat": false,
      "new_value": null,
      "is_ambiguous": false,
      "is_dangerous": false
    }
  ]
}

注意：当前没有可用的候选配置项。target_config_key 只能填 null。
完全不输出 JSON 以外的任何文字。${historyBlock}`
  }

  return UNDERSTANDING_AGENT_PROMPT.replace('{{INJECTED_CANDIDATES}}', injectedContext) + historyBlock
}

/**
 * 解析 UA 新格式输出（3意图 + target_config_key）。
 * 兼容旧格式的 subject/intent 字段，方便过渡期测试。
 */
export function parseUA3IntentOutput(raw: string): {
  todo: Array<{
    target_config_key: string | null
    intent: string
    is_chat: boolean
    new_value: string | null
    is_ambiguous: boolean
    is_dangerous: boolean
  }>
} | null {
  try {
    const jsonMatch = raw.match(/\{[\s\S]*"todo"[\s\S]*\}/)
    if (!jsonMatch) return null
    const parsed = JSON.parse(jsonMatch[0])
    if (!parsed.todo || !Array.isArray(parsed.todo)) return null
    return {
      todo: parsed.todo.map((item: Record<string, unknown>) => ({
        target_config_key: (item.target_config_key as string) ?? null,
        intent: String(item.intent || 'QUERY'),
        is_chat: !!item.is_chat,
        new_value: (item.new_value as string) ?? null,
        is_ambiguous: !!item.is_ambiguous,
        is_dangerous: !!item.is_dangerous,
      })),
    }
  } catch {
    return null
  }
}
