// === AI System Shared Types ===

export type AIProviderType = 'openai' | 'deepseek' | 'ollama'

export interface PlanStep {
  id: number
  text: string
  done: boolean
  toolName?: string
  result?: string
  /** 关联 ToolCallRegistry 记录的回执编号（证据链配平 key） */
  callId?: string
  /** 多 tool 步骤的所有 callId（roundVerify 校验收所有 tool 的 Registry 记录） */
  allCallIds?: string[]
  /** 证据链配平状态：pending=待配平 / matched=已匹配 / unmatched=未匹配 */
  evidenceStatus?: 'pending' | 'matched' | 'unmatched'
}

export interface AIMessage {
  role: 'system' | 'user' | 'assistant' | 'tool'
  content: string
  tool_call_id?: string
  name?: string
  /** TodoList: 计划步骤列表（assistant 消息可选携带） */
  planSteps?: PlanStep[]
  /** Thinking: 当前正在执行的思考文本（assistant 消息可选携带） */
  thinking?: string
  /** 结构化 tool 卡片数据（方案A：替代 content 中的 tool called/return 文本行） */
  toolCards?: import('./components/SandboxTodo').ToolCard[]
  /** UA 原始解析输出快照（方案C 修正前），供测试断言验证 UA 分类准确性 */
  uaRawOutput?: { todo: Array<{ subject: string; intent: string; value: string | null; condition: string | null; is_chat: boolean }> }
}

export interface FunctionToolParam {
  name: string
  description: string
  parameters: Record<string, unknown>
}

export interface ToolCall {
  id: string
  type: 'function'
  function: {
    name: string
    arguments: string
  }
}

export interface AICompletionRequest {
  messages: AIMessage[]
  tools?: FunctionToolParam[]
  temperature?: number
  max_tokens?: number
  /** P4: 可选模型覆盖（快速分类用小模型，执行用主模型） */
  model?: string
}

export interface AITokenUsage {
  prompt_tokens: number
  completion_tokens: number
  total_tokens: number
}

export interface AICompletionResponse {
  message: AIMessage
  tool_calls?: ToolCall[]
  usage?: AITokenUsage
}

export interface ToolResult {
  success: boolean
  data?: unknown
  error?: string
}

export interface FunctionTool {
  definition: FunctionToolParam
  execute(args: Record<string, unknown>): Promise<ToolResult>
  /** 自定义结果渲染函数，沙箱中每条数据渲染为一行可读文本 */
  resultRenderer?: (data: unknown) => string[]
}

export interface AIBridgeConfig {
  provider: AIProviderType
  apiKey?: string
  baseUrl?: string
  model?: string
  ollamaUrl?: string
  ollamaModel?: string
  /** EMB: embedding 模型名（默认同 model/ollamaModel，可在设置中修改） */
  embeddingModel?: string
}

// Validation result from BlessStar Schema/Gate
export interface ValidationResult {
  valid: boolean
  errors: ValidationError[]
}

export interface ValidationError {
  path: string
  message: string
  code?: string
}

export const MAX_TOOL_RETRIES = 3

// ── Pre-Gate: 工具入参前置校验规则 ───────────────────────────────────

/**
 * Pre-Gate 规则类型：在工具执行前，用 Gate 风格的声明式规则校验入参。
 * 对应缺口四（AI-02/03「只校验输出，不校验输入」）的 BlessStar-native 方案。
 */
export interface PreGateRule {
  /** 规则类型 */
  type: 'not_empty' | 'regex_match' | 'regex_not_match' | 'custom'
  /** 要校验的字段名 */
  field: string
  /** 正则 pattern（仅 regex_match / regex_not_match 使用） */
  pattern?: string
  /** 自定义校验函数名（仅 custom 使用，注册在 preGateEvaluators 中） */
  evaluator?: string
  /** 校验失败的错误消息 */
  error: string
}

/**
 * Pre-Gate 规则集合（按工具名索引）
 */
export type PreGateRulesMap = Record<string, PreGateRule[]>

// ── Tool Result Schema: 工具结果声明式格式化 ──────────────────────────

/**
 * Tool Result Schema：每工具声明结果 Schema，delta 格式化器从声明自动推导。
 * 对应缺口二（CTX-03「tool delta 覆盖率无保障」）的 BlessStar-native 方案。
 *
 * 与 bs_field_decl 同构——字段声明方式一致。
 */
export interface ToolResultSchemaField {
  name: string
  type: 'string' | 'number' | 'boolean' | 'array'
  label: string
  priority: number  // 1=最高
}

export interface ToolResultSchema {
  fields: ToolResultSchemaField[]
  successTemplate: string    // "📂 {path} ({count} 项)"
  emptyTemplate?: string     // "📂 {path}: 空目录"
  errorTemplate?: string     // "❌ {toolName}: {error}"
}

// ── Tool Declaration: 工具全量声明（合并缺口二+七）─────────────────────

/**
 * Tool Declaration：一份声明，工厂自动生成全套组件。
 * 对应缺口七（工具注册清单无强制约束）的 BlessStar-native 方案。
 *
 * 与 Agent Factory 同构——声明式定义驱动工厂代码生成。
 */
export interface ToolDeclaration {
  name: string
  description: string
  params: Record<string, ToolParamDecl>
  resultSchema: ToolResultSchema
  preGates?: PreGateRule[]
  category?: ToolCategory
  allowedCallers?: string[]
  approvalRequired?: boolean
  /** 自定义结果渲染函数，覆盖自动从 resultSchema 生成的渲染逻辑 */
  resultRenderer?: (data: unknown) => string[]
  execute(args: Record<string, unknown>): Promise<ToolResult>
}

export interface ToolParamDecl {
  type: string
  description: string
  required?: boolean
}

// ── Tool Category: 工具分类（对应专题六三类 Tool）─────────────────────

/**
 * 工具分类：
 * - retrieval: 检索类，轻量上下文注入，不修改系统状态
 * - terminal: 终端类，受限命令执行，受命令白名单 + 目录沙箱约束（GAP-12）
 * - execution: 执行类，可修改系统状态，需用户确认（approval_required）
 */
export type ToolCategory = 'retrieval' | 'terminal' | 'execution'

// ── Business Paradigm: 业务系统翻译模板（对应 GAP-10）─────────────────

/**
 * 每个 tool 在不同业务系统中的自然语言翻译模板。
 * 用户可见的翻译全部走模板，零 AI 翻译幻觉。
 */
export interface ToolTranslationTemplates {
  announcing?: string  // "正在查看 {path} 目录..."
  success: string      // "📂 已找到 {count} 个模型：{entries}"
  failure?: string     // "❌ 目录 {path} 不存在或无权访问"
  empty?: string       // "📂 {path} 下暂无模型文件"
}

/**
 * 业务范式声明：一个业务系统对 tool 结果的翻译模板集合。
 * 只记录与通用模板不同的覆写（GAP-15 范式蒸馏后支持 delta 模式）。
 */
export interface BusinessParadigm {
  systemName: string
  systemType?: string
  toolTemplates: Record<string, ToolTranslationTemplates>
  overrides?: Partial<BusinessParadigm>  // delta 增量（范式蒸馏用）
}

export interface ToolDelta {
  summary: string
}

// ── Execution Trace DAG: 工具执行轨迹（对应缺口三）───────────────────

/**
 * 工具执行轨迹节点。
 * 对应缺口三（CTX Layer 1「单 tool delta 槽位」）的 BlessStar-native 方案。
 * 存工具执行轨迹有向无环图：节点=调用，边=数据依赖。
 */
export interface TraceNode {
  callId: string
  toolName: string
  input: Record<string, unknown>
  outputSummary: string
  dependsOn: string[]
}

export interface ExecutionTrace {
  round: number
  nodes: TraceNode[]
}

/**
 * Tool Call Registry — 工具调用执行证据库。
 * 对应缺口五（无工具调用 grounding）的 BlessStar-native 方案。
 * 每个工具执行存入 Registry，AI 声明必须有 Registry 记录作为证据。
 */
export interface ToolCallRecord {
  callId: string
  toolName: string
  outputHash: string
  status: 'success' | 'failed'
  timestamp: number
}

export interface VerificationResult {
  verified: boolean
  reason?: string
}

// ── 配置级版本注册表（第33天 · RV-01 配置级平坦映射）────────────────

/** 单个配置版本的条目 */
export interface ConfigVersion {
  versionId: string        // "{configKey}_v{序号}"
  displayName: string      // 用户命名，空串 = 使用默认版本号
  value: string            // 该版本的配置值
  timestamp: number        // 创建时间
  userInput: string        // 触发该版本的用户对话原文
}

/** 版本注册表：configKey → 版本数组的平坦映射 */
export type VersionRegistry = Record<string, ConfigVersion[]>

/** 一次管线写入的条目（用于版本保存） */
export interface WriteEntry {
  key: string
  value: string
  displayName?: string
}
