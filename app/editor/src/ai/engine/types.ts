/**
 * ADR-全链路接通 — AI Pipeline Engine core types.
 *
 * Architecture invariances:
 *  - #1 (双向依赖禁止): Engine → IDomainPlugin only. No reverse imports.
 *  - #4 (全链路 TraceID): Every PipelineContext carries a traceId.
 *  - #7 (管线可观测): ExecutionTrace records all stage/tool executions.
 */

export interface PipelineContext {
  traceId: string
  userMessage: string
  clauses: Clause[]
  currentIntent: Intent | null
  toolResults: ToolResult[]
  domainPlugin: IDomainPlugin
  abortSignal: AbortSignal
  metadata: Record<string, unknown>
}

export interface Clause {
  text: string
  domain?: string
  intent?: string
  confidence: number
}

export interface Intent {
  action: 'read' | 'write' | 'delete' | 'list' | 'validate' | 'gate' | 'schema'
  subject: string        // e.g. "timeout", "server.port"
  target: string         // e.g. "src/prod.yaml"
  newValue?: unknown
  confidence: number
}

export interface StageResult {
  stageId: string
  status: 'success' | 'error' | 'skipped'
  output?: unknown
  error?: string
  durationMs: number
}

export interface ToolResult {
  toolName: string
  status: 'success' | 'error'
  data?: unknown
  error?: string
  durationMs: number
}

export interface ExecutionTrace {
  traceId: string
  stages: Array<{
    stageId: string
    status: string
    durationMs: number
    toolCalls: Array<{
      toolName: string
      durationMs: number
      status: string
    }>
  }>
  startTime: number
  endTime: number
}

export interface IToolDeclaration {
  name: string
  description: string
  parameters: Record<string, unknown>
  execute(args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult>
}

export interface IPipelineStage {
  id: string
  execute(ctx: PipelineContext): Promise<StageResult>
}

export interface ToolSystem {
  registerTool(tool: IToolDeclaration): void
  executeTool(name: string, args: Record<string, unknown>, ctx: PipelineContext): Promise<ToolResult>
  getTools(): IToolDeclaration[]
}

/** IDomainPlugin — defined here as type only; actual impl in domain-plugin/ */
export interface IDomainPlugin {
  id: string
  version: string
  supportedPipelineVersion: string
  tools: IToolDeclaration[]
  getSystemPrompt(): string
  getConceptKnowledge(): IConceptEntry[]
  getTrieDictionary(): ITrieDict
  getCommandRegistry(): ICommandEntry[]
  getPersistenceProvider(): IPersistenceProvider
  hooks?: {
    onBeforeStage?: (stageId: string, ctx: PipelineContext) => Promise<void>
    onAfterStage?: (stageId: string, result: StageResult, ctx: PipelineContext) => Promise<void>
    onToolResult?: (toolName: string, result: ToolResult, ctx: PipelineContext) => Promise<void>
  }
}

export interface IConceptEntry {
  key: string
  description: string
  keywords: string[]
}

export interface ITrieDict {
  domainKW: Record<string, string>
  opKW: Record<string, string>
  opMap: Record<string, string>
}

export interface ICommandEntry {
  command: string
  description: string
  handler: (args: string[], ctx: PipelineContext) => Promise<string>
}

export interface IPersistenceProvider {
  save(data: unknown, path: string): Promise<void>
  load(path: string): Promise<unknown>
}
