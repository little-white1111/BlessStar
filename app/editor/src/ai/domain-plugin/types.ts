/**
 * IDomainPlugin interface — contracts for domain-specific AI behaviors.
 *
 * ADR-全链路接通 不变量 #1 (双向依赖禁止):
 *   Engine → IDomainPlugin only. Plugins must NOT import engine internals.
 * ADR-全链路接通 不变量 #3 (Plugin 隔离性):
 *   Each plugin runs in its own namespace.
 * ADR-全链路接通 不变量 #9 (Plugin 版本契约):
 *   version must match supportedPipelineVersion.
 */

import type {
  IToolDeclaration,
  IConceptEntry,
  ITrieDict,
  ICommandEntry,
  IPersistenceProvider,
  PipelineContext,
  StageResult,
  ToolResult,
} from '../engine/types'

export type { IToolDeclaration, IConceptEntry, ITrieDict, ICommandEntry, IPersistenceProvider }

export interface IDomainPlugin {
  id: string
  version: string
  supportedPipelineVersion: string

  /** Domain-specific tools. */
  tools: IToolDeclaration[]

  /** System prompt injected into LLM context. */
  getSystemPrompt(): string

  /** Concept knowledge for context building. */
  getConceptKnowledge(): IConceptEntry[]

  /** Trie dictionary for deterministic intent matching. */
  getTrieDictionary(): ITrieDict

  /** Command registry for /command style interactions. */
  getCommandRegistry(): ICommandEntry[]

  /** Persistence provider for saving/loading configs. */
  getPersistenceProvider(): IPersistenceProvider

  /** Optional lifecycle hooks. */
  hooks?: {
    onBeforeStage?: (stageId: string, ctx: PipelineContext) => Promise<void>
    onAfterStage?: (stageId: string, result: StageResult, ctx: PipelineContext) => Promise<void>
    onToolResult?: (toolName: string, result: ToolResult, ctx: PipelineContext) => Promise<void>
  }
}
