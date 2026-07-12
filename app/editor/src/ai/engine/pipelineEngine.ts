/**
 * PipelineEngine — generic pipeline orchestrator.
 *
 * Executes a sequence of IPipelineStage instances, passing a PipelineContext
 * through each stage. Domain-agnostic — all domain logic comes from IDomainPlugin.
 *
 * ADR-全链路接通 不变量 #1 (双向依赖禁止):
 *   Engine → IDomainPlugin only. DomainPlugin does NOT import engine internals.
 * ADR-全链路接通 不变量 #4 (全链路 TraceID):
 *   Each pipeline run generates a unique traceId.
 * ADR-全链路接通 不变量 #7 (管线可观测):
 *   Each stage records duration, status, and tool calls.
 */

import type { IPipelineStage, PipelineContext, StageResult } from './types'

function generateTraceId(): string {
  return `trace-${Date.now()}-${Math.random().toString(36).slice(2, 10)}`
}

export class PipelineEngine {
  private stages: IPipelineStage[] = []
  private onTraceCallback?: (trace: unknown) => void

  setStages(stages: IPipelineStage[]): void {
    this.stages = stages
  }

  addStage(stage: IPipelineStage): void {
    this.stages.push(stage)
  }

  onTrace(cb: (trace: unknown) => void): void {
    this.onTraceCallback = cb
  }

  async execute(ctx: Partial<PipelineContext>): Promise<{
    results: StageResult[]
    traceId: string
  }> {
    const traceId = generateTraceId()
    const fullCtx: PipelineContext = {
      traceId,
      userMessage: ctx.userMessage ?? '',
      clauses: ctx.clauses ?? [],
      currentIntent: ctx.currentIntent ?? null,
      toolResults: [],
      domainPlugin: ctx.domainPlugin!,
      abortSignal: ctx.abortSignal ?? new AbortController().signal,
      metadata: ctx.metadata ?? {},
    }

    const trace = {
      traceId,
      stages: [] as Array<{
        stageId: string
        status: string
        durationMs: number
        toolCalls: Array<{ toolName: string; durationMs: number; status: string }>
      }>,
      startTime: Date.now(),
      endTime: 0,
    }

    const results: StageResult[] = []

    for (const stage of this.stages) {
      if (fullCtx.abortSignal.aborted) {
        results.push({
          stageId: stage.id,
          status: 'skipped',
          durationMs: 0,
          error: 'Pipeline aborted',
        })
        continue
      }

      // Pre-stage hook
      if (fullCtx.domainPlugin?.hooks?.onBeforeStage) {
        await fullCtx.domainPlugin.hooks.onBeforeStage(stage.id, fullCtx)
      }

      const stageStart = performance.now()
      let result: StageResult

      try {
        result = await stage.execute(fullCtx)
      } catch (err) {
        result = {
          stageId: stage.id,
          status: 'error',
          error: err instanceof Error ? err.message : String(err),
          durationMs: performance.now() - stageStart,
        }
      }

      result.durationMs = performance.now() - stageStart
      results.push(result)

      // Post-stage hook
      if (fullCtx.domainPlugin?.hooks?.onAfterStage) {
        await fullCtx.domainPlugin.hooks.onAfterStage(stage.id, result, fullCtx)
      }

      trace.stages.push({
        stageId: stage.id,
        status: result.status,
        durationMs: result.durationMs,
        toolCalls: fullCtx.toolResults.map(tr => ({
          toolName: tr.toolName,
          durationMs: tr.durationMs,
          status: tr.status,
        })),
      })

      // Stop on error
      if (result.status === 'error') break
    }

    trace.endTime = Date.now()
    if (this.onTraceCallback) {
      this.onTraceCallback(trace)
    }

    return { results, traceId }
  }
}
