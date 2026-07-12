/**
 * engine/stage — 通用 Pipeline Stage 实现（ai-engine-core）
 *
 * ADR-全链路接通 — 通用 Stage 定义，通过 IDomainPlugin 获取领域数据。
 * 各 Stage 导入并调用 pipeline/ 目录的真实管线函数，确保功能与原 20 步管线一致。
 *
 * Router → Intent → Execute → Render
 *
 * 架构不变量 #1 (双向依赖禁止):
 *   Stage → IDomainPlugin (引擎调用接口)，禁止 DomainPlugin 反向 import 引擎内部状态
 *
 * 架构不变量 #4 (全链路 TraceID):
 *   每个 PipelineContext 携带 traceId
 *
 * 架构不变量 #7 (管线可观测):
 *   每个 Stage 记录 duration、status、tool calls
 */

import type { IPipelineStage, PipelineContext, StageResult } from './types'

// Stage 1 — Router
export class StageRouter implements IPipelineStage {
  id = 'router'

  async execute(ctx: PipelineContext): Promise<StageResult> {
    const start = performance.now()
    try {
      // 委托给 pipeline/stage-router 的真实实现
      const { executeStageRouter } = await import('../../pipeline/stage-router')
      const { queryAllLayers, resolveRoute } = await import('../../context-manager/adaptiveIndex')

      const text = ctx.userMessage
      // ── 在后设资料中储存管线上下文 ──
      const { createPipelineContext } = await import('../../pipeline/types')
      const pCtx = createPipelineContext(text)
      ctx.metadata.pipelineCtx = pCtx

      // ① L0 采集 + skillMatch
      executeStageRouter(pCtx)
      ctx.clauses = pCtx.clauses.map(c => ({ text: c, confidence: 1.0 }))

      // 概念路由
      const routeHits = queryAllLayers(text)
      const { configCandidates, conceptHit } = resolveRoute(routeHits)

      ctx.metadata.concept = { configCandidates, conceptHit }

      return {
        stageId: this.id,
        status: 'success',
        output: {
          clauseCount: pCtx.clauses.length,
          skillMatched: pCtx.skillMatch.matched,
          conceptHit: conceptHit?.conceptId ?? null,
        },
        durationMs: performance.now() - start,
      }
    } catch (err) {
      return {
        stageId: this.id,
        status: 'error',
        error: err instanceof Error ? err.message : String(err),
        durationMs: performance.now() - start,
      }
    }
  }
}

// Stage 2 — Intent
export class StageIntent implements IPipelineStage {
  id = 'intent'

  async execute(ctx: PipelineContext): Promise<StageResult> {
    const start = performance.now()
    try {
      const { executeStageIntent } = await import('../../pipeline/stage-intent')
      const pCtx = ctx.metadata.pipelineCtx as any
      if (!pCtx) {
        return { stageId: this.id, status: 'skipped', output: 'No pipeline context', durationMs: 0 }
      }

      // 执行意图解析 (Think Level + ⑩a)
      await executeStageIntent(pCtx)

      // 使用 DomainPlugin 的 system prompt 丰富领域上下文
      const sysPrompt = ctx.domainPlugin?.getSystemPrompt?.() || ''
      ctx.metadata.systemPrompt = sysPrompt

      return {
        stageId: this.id,
        status: 'success',
        output: {
          thinkLevel: pCtx.thinkLevel,
          hintsCollected: !!pCtx.hints,
        },
        durationMs: performance.now() - start,
      }
    } catch (err) {
      return {
        stageId: this.id,
        status: 'error',
        error: err instanceof Error ? err.message : String(err),
        durationMs: performance.now() - start,
      }
    }
  }
}

// Stage 3 — Execute
export class StageExecute implements IPipelineStage {
  id = 'execute'

  async execute(ctx: PipelineContext): Promise<StageResult> {
    const start = performance.now()
    try {
      const { executeStage } = await import('../../pipeline/stage-execute')
      const pCtx = ctx.metadata.pipelineCtx as any
      if (!pCtx) {
        return { stageId: this.id, status: 'skipped', output: 'No pipeline context', durationMs: 0 }
      }

      const toolCalls = pCtx.toolCallsToExecute || []
      const planSteps = pCtx.planSteps || []
      const planStepRanges = pCtx.planStepToolRanges || []

      if (toolCalls.length === 0) {
        return {
          stageId: this.id,
          status: 'success',
          output: { executedCount: 0 },
          durationMs: performance.now() - start,
        }
      }

      const result = await executeStage(
        toolCalls,
        planSteps,
        planStepRanges,
        pCtx.isUA ?? false,
        pCtx.cleanContent ?? '',
      )

      ctx.metadata.execResult = result
      ctx.metadata.planSteps = planSteps
      ctx.toolResults = result.toolResults.map(r => ({
        toolName: r.toolName ?? '',
        status: r.success ? 'success' : 'error',
        data: r.data,
        error: r.error,
        durationMs: r.durationMs ?? 0,
      }))

      return {
        stageId: this.id,
        status: 'success',
        output: {
          executedCount: toolCalls.length,
          allSuccess: result.allToolSuccess,
          fabricationWarning: result.fabricationWarning,
        },
        durationMs: performance.now() - start,
      }
    } catch (err) {
      return {
        stageId: this.id,
        status: 'error',
        error: err instanceof Error ? err.message : String(err),
        durationMs: performance.now() - start,
      }
    }
  }
}

// Stage 4 — Render
export class StageRender implements IPipelineStage {
  id = 'render'

  async execute(ctx: PipelineContext): Promise<StageResult> {
    const start = performance.now()
    try {
      const pCtx = ctx.metadata.pipelineCtx as any
      const execResult = ctx.metadata.execResult as any
      const planSteps = ctx.metadata.planSteps as any[] || []

      if (!execResult || !pCtx) {
        return {
          stageId: this.id,
          status: 'success',
          output: { summary: '跳过渲染（无执行结果）' },
          durationMs: performance.now() - start,
        }
      }

      // 构建自然语言总结
      const allOk = planSteps.every((_s: any, i: number) =>
        execResult.toolResults?.[i]?.success ?? false
      )

      // 使用 DomainPlugin 的 system prompt 丰富回复
      const summary = allOk
        ? `✅ 已完成 ${execResult.toolResults?.length || 0} 个工具调用。`
        : `⚠️ 部分工具执行失败，请检查详情。`

      const output = {
        summary,
        toolCards: execResult.toolCards || [],
        allToolSuccess: execResult.allToolSuccess ?? false,
        verifyResult: execResult.verifyResult,
        fabricationWarning: execResult.fabricationWarning,
      }

      ctx.metadata.renderOutput = output

      return {
        stageId: this.id,
        status: 'success',
        output,
        durationMs: performance.now() - start,
      }
    } catch (err) {
      return {
        stageId: this.id,
        status: 'error',
        error: err instanceof Error ? err.message : String(err),
        durationMs: performance.now() - start,
      }
    }
  }
}
