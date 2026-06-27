/**
 * skillWorkflow — Skill 工作流执行器（P1-4）
 *
 * 对应 GAP-08（Skill Router 优先——/command 命中则跳过全部下层）。
 * 当 matchSkill() 命中后，由本执行器按 toolChain 顺序执行工作流。
 *
 * 工作流执行策略：
 * - 线性步骤，不支持 if-else 条件跳转（MVP）
 * - 每步执行前先 Pre-Gate 校验
 * - 每步执行后记录 ExecutionTrace + ToolCallRegistry
 * - 所有步骤失败时停止并报告
 */

import { matchSkill, parseCommand, type SkillMatch, UNIFIED_SKILLS } from './skillRouter'
import { findTool } from '../executor'
import { evaluatePreGates, TOOL_PRE_GATE_RULES } from '../preGate'
import { executionTrace, toolCallRegistry } from './executionTrace'
import type { ToolResult } from '../types'

// ── 类型 ──────────────────────────────────────────────────────────────

export interface WorkflowStepResult {
  toolName: string
  success: boolean
  result: ToolResult
  callId: string
}

export interface WorkflowResult {
  matched: boolean
  skillName?: string
  approvalRequired?: boolean
  steps: WorkflowStepResult[]
  allSuccess: boolean
  summary: string
}

// ── 执行器 ────────────────────────────────────────────────────────────

/**
 * 执行一个 Skill 工作流。
 *
 * 1. 匹配 Skill 路由
 * 2. 按 toolChain 顺序执行每个工具
 * 3. 记录到 ExecutionTrace + ToolCallRegistry
 * 4. 返回 WorkflowResult
 *
 * @param input 用户输入（含 /command 前缀）
 * @param approvalGranted 用户是否已确认授权（仅限 approvalRequired 的 skill）
 */
export async function executeSkillWorkflow(
  input: string,
  approvalGranted: boolean = false,
): Promise<WorkflowResult> {
  // Step 1: 匹配 Skill（兼容旧 SKILL_ROUTES 和 UNIFIED_SKILLS）
  const skillMatch: SkillMatch = matchSkill(input)

  // 如果旧路由未命中，尝试 UNIFIED_SKILLS 的 parseCommand
  if (!skillMatch.matched) {
    const parsed = parseCommand(input)
    if (parsed.matched) {
      // 从 UNIFIED_SKILLS 找出匹配的 skill 定义
      const unified = UNIFIED_SKILLS.find(
        s => s.triggers.exactCommands.includes('/' + parsed.command),
      )
      if (unified) {
        // 构建虚拟 SkillMatch
        const virtualMatch: SkillMatch & { skillName?: string; virtualRoute?: { prefix: string; toolChain: string[]; approvalRequired?: boolean } } = {
          matched: true,
          skillName: parsed.command,
          virtualRoute: {
            prefix: '/' + parsed.command,
            toolChain: unified.executor,
            approvalRequired: (unified as { approvalRequired?: boolean }).approvalRequired,
          },
        }
        return executeWithRoute(input, approvalGranted, virtualMatch)
      }
    }
    return { matched: false, steps: [], allSuccess: false, summary: '' }
  }

  return executeWithRoute(input, approvalGranted, skillMatch)
}

async function executeWithRoute(
  input: string,
  approvalGranted: boolean,
  skillMatch: SkillMatch & { virtualRoute?: { prefix: string; toolChain: string[]; approvalRequired?: boolean } },
): Promise<WorkflowResult> {
  const route = skillMatch.route || skillMatch.virtualRoute!

  // Step 2: 检查是否需要用户确认
  if (route.approvalRequired && !approvalGranted) {
    return {
      matched: true,
      skillName: route.prefix,
      approvalRequired: true,
      steps: [],
      allSuccess: false,
      summary: `⚠️ Skill ${route.prefix} 需要用户确认后才能执行。`,
    }
  }

  // Step 3: 开始 Execution Trace 新轮次
  executionTrace.newRound()
  const steps: WorkflowStepResult[] = []
  let allSuccess = true

  for (const toolName of route.toolChain) {
    const tool = findTool(toolName)
    if (!tool) {
      const errResult: WorkflowStepResult = {
        toolName,
        success: false,
        result: { success: false, error: `未知工具: ${toolName}` },
        callId: '',
      }
      steps.push(errResult)
      allSuccess = false
      break
    }

    // 构造参数：从用户输入中的 params 提取（可扩展为参数解析）
    const args: Record<string, unknown> = {}
    if (skillMatch.params) {
      // 简单参数解析：/createconfig name=amount type=i32
      const paramRegex = /(\w+)\s*=\s*(\S+)/g
      let pm: RegExpExecArray | null
      while ((pm = paramRegex.exec(skillMatch.params)) !== null) {
        args[pm[1]] = pm[2]
      }
    }

    // Pre-Gate 校验
    const preGateError = evaluatePreGates(TOOL_PRE_GATE_RULES[toolName], args)
    if (preGateError !== null) {
      const errResult: WorkflowStepResult = {
        toolName,
        success: false,
        result: { success: false, error: `[Pre-Gate] ${preGateError}` },
        callId: '',
      }
      steps.push(errResult)
      allSuccess = false
      break
    }

    // 执行工具
    const result = await tool.execute(args)

    // 记录到 Trace
    const outputSummary = result.success
      ? `✅ ${toolName} 成功`
      : `❌ ${toolName}: ${result.error || '失败'}`
    const traceNode = executionTrace.addNode({
      toolName,
      input: args,
      outputSummary,
      dependsOn: steps.length > 0 ? [steps[steps.length - 1].callId] : [],
    })

    // 记录到 Registry
    toolCallRegistry.record(
      traceNode.callId,
      toolName,
      outputSummary,
      result.success ? 'success' : 'failed',
    )

    steps.push({
      toolName,
      success: result.success,
      result,
      callId: traceNode.callId,
    })

    if (!result.success) {
      allSuccess = false
      break
    }
  }

  // Step 4: 构建摘要
  const summary = buildWorkflowSummary(route.prefix, steps, allSuccess)

  return {
    matched: true,
    skillName: route.prefix,
    approvalRequired: route.approvalRequired,
    steps,
    allSuccess,
    summary,
  }
}

/**
 * 构建工作流执行摘要文本
 */
function buildWorkflowSummary(skillName: string, steps: WorkflowStepResult[], allSuccess: boolean): string {
  const lines: string[] = [`🔧 Skill: ${skillName}`]

  for (const step of steps) {
    const icon = step.success ? '✅' : '❌'
    lines.push(`  ${icon} ${step.toolName} (${step.callId || '未执行'})`)
    if (!step.success && step.result.error) {
      lines.push(`    错误: ${step.result.error}`)
    }
  }

  if (allSuccess) {
    lines.push(`🎉 Skill ${skillName} 全部步骤执行成功`)
  } else {
    lines.push(`⚠️ Skill ${skillName} 执行失败，请重试`)
  }

  return lines.join('\n')
}
