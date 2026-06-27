/**
 * executionTrace — 工具执行轨迹管理器（Execution Trace DAG）
 *
 * 对应缺口三（CTX Layer 1「单 tool delta 槽位」）的 BlessStar-native 方案。
 * 存工具执行轨迹有向无环图：节点=调用，边=数据依赖（read→list）。
 * 与 Gate 链 DAG 序列化（AGF-OPT-02）复用同一套基础设施概念。
 *
 * 对应缺口五（无工具调用 grounding）的 Tool Call Registry 共处一地。
 */

import type { TraceNode, ExecutionTrace, ToolCallRecord, VerificationResult, PlanStep } from '../types'

// ── Execution Trace ──────────────────────────────────────────────────

export class ExecutionTraceManager {
  private round = 0
  private nodes: TraceNode[] = []

  /** 开始新一轮追踪 */
  newRound(): void {
    this.round++
  }

  /** 添加一个工具调用节点 */
  addNode(node: Omit<TraceNode, 'callId' | 'dependsOn'> & { callId?: string; dependsOn?: string[] }): TraceNode {
    const traceNode: TraceNode = {
      callId: node.callId || `call_${this.toolNameToShort(node.toolName)}_${String(this.nodes.length + 1).padStart(2, '0')}`,
      toolName: node.toolName,
      input: node.input,
      outputSummary: node.outputSummary,
      dependsOn: node.dependsOn || [],
    }
    this.nodes.push(traceNode)
    return traceNode
  }

  /** 获取当前轮的完整 trace */
  getCurrentTrace(): ExecutionTrace {
    return { round: this.round, nodes: [...this.nodes] }
  }

  /** 序列化为上下文文本（注入下轮 AI） */
  serialize(): string {
    if (this.nodes.length === 0) return ''

    const lines: string[] = ['工具执行轨迹:']
    for (const node of this.nodes) {
      const depInfo = node.dependsOn.length > 0 ? ` (依赖: ${node.dependsOn.join(', ')})` : ''
      lines.push(`  [${node.callId}] ${node.toolName} → ${node.outputSummary}${depInfo}`)
    }
    return lines.join('\n')
  }

  /** 重置（跨轮清理） */
  reset(maxKeep: number = 1): void {
    // 只保留最近 maxKeep 轮的节点
    if (this.round > maxKeep) {
      this.nodes = this.nodes.slice(-10 * maxKeep) // 约每轮 10 节点
    }
  }

  private toolNameToShort(name: string): string {
    const parts = name.split('_')
    return parts.length >= 2 ? parts.map((p) => p[0]).join('') : name.slice(0, 3)
  }
}

// ── Tool Call Registry ───────────────────────────────────────────────

function sha256(input: string): string {
  // 简化：使用内置的简单 hash（MVP 阶段，生产环境应使用 SubtleCrypto）
  let hash = 0
  for (let i = 0; i < input.length; i++) {
    const char = input.charCodeAt(i)
    hash = ((hash << 5) - hash) + char
    hash |= 0 // Convert to 32bit integer
  }
  return Math.abs(hash).toString(16).slice(0, 8)
}

export class ToolCallRegistry {
  private records = new Map<string, ToolCallRecord>()

  /** 记录一条工具调用 */
  record(callId: string, toolName: string, output: string, status: 'success' | 'failed'): ToolCallRecord {
    const record: ToolCallRecord = {
      callId,
      toolName,
      outputHash: sha256(output),
      status,
      timestamp: Date.now(),
    }
    this.records.set(callId, record)
    return record
  }

  /** 按 callId 查找 */
  findByCallId(id: string): ToolCallRecord | null {
    return this.records.get(id) || null
  }

  /** 验证声称的输出是否与 Registry 记录一致 */
  verifyOutput(callId: string, claimedOutput: string): boolean {
    const record = this.findByCallId(callId)
    if (!record) return false
    return record.outputHash === sha256(claimedOutput).slice(0, 8)
  }

  /** 验证 AI 响应中的工具声称：必须有 Registry 记录作为证据 */
  verifyAIClaims(text: string): VerificationResult {
    const callIds = this.extractCallIds(text)
    const hasToolClaims = /(已列出|已读取|已创建|已更新|已写入|已生成)/.test(text)

    if (hasToolClaims && callIds.length === 0) {
      return { verified: false, reason: 'AI 声称使用了工具但无回执编号' }
    }

    for (const id of callIds) {
      if (!this.findByCallId(id)) {
        return { verified: false, reason: `回执 ${id} 不存在` }
      }
    }

    return { verified: true }
  }

  /** 从文本中提取 [call_xxx] 引用 */
  private extractCallIds(text: string): string[] {
    const regex = /\[(call_[a-z0-9_]+)\]/g
    const ids: string[] = []
    let match: RegExpExecArray | null
    while ((match = regex.exec(text)) !== null) {
      ids.push(match[1])
    }
    return ids
  }

  /** 重置所有记录（跨测试清理） */
  reset(): void {
    this.records.clear()
  }
}

// ── 全局单例 ──────────────────────────────────────────────────────────

export const executionTrace = new ExecutionTraceManager()
export const toolCallRegistry = new ToolCallRegistry()

// ── 证据链强制配平（专题四 · SBOX-05~08）─────────────────────────────

/**
 * 轮次结束证据配平验证。
 *
 * 对每步 done=true 的 planStep 进行 callId → ToolCallRegistry 配对，
 * 同时验证 AI 总结文本中的完成声称是否在 Registry 中有对应记录。
 *
 * @param planSteps - 本轮所有计划步骤（含 done 状态）
 * @param aiSummaryText - AI 生成的总结文本（可为空字符串跳过验证）
 * @returns { allMatched, unmatchedSteps, summaryValid }
 */
export function roundVerify(
  planSteps: PlanStep[],
  aiSummaryText: string,
): { allMatched: boolean; unmatchedSteps: PlanStep[]; summaryValid: boolean } {
  const unmatchedSteps: PlanStep[] = []

  // ── 逐步配平 ──
  for (const step of planSteps) {
    if (!step.done) continue // 未完成的不参与配平

    // CHAT 步骤无工具调用，跳过 Registry 配平
    if (step.text.startsWith('[CHAT]')) {
      step.evidenceStatus = 'matched'
      continue
    }

    // 多 tool 步骤：全量校验所有 tool 的 Registry 记录
    if (step.allCallIds && step.allCallIds.length > 0) {
      const allOk = step.allCallIds.every(cid => {
        const rec = toolCallRegistry.findByCallId(cid)
        return rec && rec.status === 'success'
      })
      if (allOk) {
        step.evidenceStatus = 'matched'
      } else {
        step.evidenceStatus = 'unmatched'
        unmatchedSteps.push(step)
      }
    } else if (step.callId) {
      const record = toolCallRegistry.findByCallId(step.callId)
      if (record && record.status === 'success') {
        step.evidenceStatus = 'matched'
      } else {
        step.evidenceStatus = 'unmatched'
        unmatchedSteps.push(step)
      }
    } else {
      // 没有 callId → 直接 unmatched（AI 声称完成但无证据）
      step.evidenceStatus = 'unmatched'
      unmatchedSteps.push(step)
    }
  }

  // ── 验证总结文本 ──
  let summaryValid = true
  if (aiSummaryText) {
    const verification = toolCallRegistry.verifyAIClaims(aiSummaryText)
    summaryValid = verification.verified
  }

  return {
    allMatched: unmatchedSteps.length === 0,
    unmatchedSteps,
    summaryValid,
  }
}

/**
 * 检测 AI 的欺诈风险：无 [PLAN]、无 tool call 执行，但文本中包含完成声称。
 *
 * 覆盖场景：AI 输出 "好的，房间号已修改为10041" 但没有制定计划也没有调用任何工具。
 *
 * @returns 风险警告字符串，无风险则返回 null
 */
export function checkFabricationRisk(
  content: string,
  planSteps: PlanStep[] | undefined,
  toolCallsExecuted: number,
): string | null {
  const fabricationMarkers = /(已修改|已写入|已完成|已创建|已更新|已删除|已设置|已生成|已读取)/
  if (!fabricationMarkers.test(content)) return null
  if (toolCallsExecuted > 0) return null
  if (planSteps && planSteps.length > 0) return null

  const match = content.match(fabricationMarkers)!
  return `⚠️ AI 声称"${match[0]}"但未调用任何工具、未制定计划。这可能是一次虚假执行。请重新提问或指定具体操作。`
}
