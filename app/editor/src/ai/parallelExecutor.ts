/**
 * parallelExecutor.ts — 并行子句执行器
 *
 * 负责将 Lexer 切分后的多个子句，并行/链式执行并聚合结果。
 * 架构方案：⑥ 并行子句执行器 — 无依赖子句并行，有父子关系链式 await。
 * 见：架构方案选择记录（第25天以后）.md § 第32天/专题二
 */

import { executeToolCall } from './executor'
import type { ToolCall, ToolResult } from './types'

/** 单个子句的执行上下文 */
export interface ClauseContext {
  /** 原始子句文本 */
  sentence: string
  /** 子句序号（从 0 开始） */
  index: number
  /** 要执行的工具调用列表 */
  toolCalls: ToolCall[]
  /** 执行结果 */
  result?: ToolResult
  /** 执行错误 */
  error?: string
}

/** 聚合后的多子句执行结果 */
export interface MultiClauseResult {
  /** 所有子句的执行结果 */
  clauses: ClauseContext[]
  /** 成功子句数 */
  succeeded: number
  /** 失败子句数 */
  failed: number
  /** 整体是否全部成功 */
  allSucceeded: boolean
}

/**
 * 切分子句：用高可靠分隔符分割用户输入
 * 守卫规则：数字内逗号不切（如 "金额大于10,000元" 视为单句）
 */
export function splitUserIntent(text: string): string[] {
  if (!text) return []
  /* 高可靠分隔符：句号 / 分号（全半角） / 中文逗号 / 换行 */
  const parts = text.split(/[。；;，\n]+/)
    .map((s) => s.trim())
    .filter((s) => s.length > 0)

  if (parts.length === 0) return []
  if (parts.length <= 1) return [text]

  /* 检查是否可能是数字内逗号导致的误切：如果原句只有 2 段且第一段末尾是数字 */
  if (parts.length === 2) {
    const firstEndsWithDigit = /\d$/.test(parts[0])
    const secondStartsWithDigit = /^\d/.test(parts[1])
    if (firstEndsWithDigit && secondStartsWithDigit) {
      return [text]
    }
  }

  return parts
}

/**
 * 并行执行多个子句
 * - 每个子句可能包含多个工具调用
 * - 无依赖子句间用 Promise.all() 并行
 * - 返回聚合结果
 */
export async function executeClauses(clauses: ClauseContext[]): Promise<MultiClauseResult> {
  /* 并行执行所有子句 */
  const results = await Promise.allSettled(
    clauses.map(async (clause) => {
      const ctx: ClauseContext = {
        sentence: clause.sentence,
        index: clause.index,
        toolCalls: clause.toolCalls,
      }

      /* 对子句内的工具调用链式执行 */
      for (const tc of clause.toolCalls) {
        try {
          ctx.result = await executeToolCall(tc)
          if (!ctx.result.success) {
            ctx.error = ctx.result.error || '工具调用失败'
            break
          }
        } catch (e) {
          ctx.error = e instanceof Error ? e.message : String(e)
          break
        }
      }
      return ctx
    }),
  )

  /* 聚合结果 */
  const aggregated: ClauseContext[] = results.map((r, i) => {
    if (r.status === 'fulfilled') return r.value
    return {
      sentence: clauses[i]?.sentence || '',
      index: i,
      toolCalls: [],
      error: r.reason instanceof Error ? r.reason.message : String(r.reason),
    }
  })

  const succeeded = aggregated.filter((c) => !c.error && c.result?.success !== false).length
  const failed = aggregated.length - succeeded

  return {
    clauses: aggregated,
    succeeded,
    failed,
    allSucceeded: failed === 0,
  }
}

/**
 * 格式化多子句执行结果为用户可读文本
 */
export function formatMultiClauseResult(result: MultiClauseResult): string {
  const lines: string[] = []

  for (const clause of result.clauses) {
    const icon = clause.error ? '❌' : (clause.result?.success !== false ? '✅' : '⚠️')
    lines.push(`${icon} 子句${clause.index + 1}: ${clause.sentence}`)
    if (clause.error) {
      lines.push(`   错误: ${clause.error}`)
    } else if (clause.result?.data) {
      const dataStr = typeof clause.result.data === 'string'
        ? clause.result.data
        : JSON.stringify(clause.result.data)
      lines.push(`   结果: ${dataStr.slice(0, 120)}`)
    }
  }

  lines.push(`\n共 ${result.clauses.length} 个子句，${result.succeeded} 成功，${result.failed} 失败`)
  return lines.join('\n')
}
