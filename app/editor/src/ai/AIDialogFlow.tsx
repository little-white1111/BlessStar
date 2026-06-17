import { useState, useCallback, useRef } from 'react'
import type { AIMessage, AICompletionRequest, ToolResult } from './types'
import { getToolDefinitions } from './tools'
import { createAIBridge } from './bridge'
import { executeToolCall } from './executor'

/* ── Types ─────────────────────────────────────────────────────────── */

type DialogPhase = 'field_define' | 'gate_edit' | 'commit'

interface DialogStep {
  id: string
  phase: DialogPhase
  prompt: string
  userConfirm: boolean
  userEdit?: string
  result?: unknown
}

interface GateChainEdit {
  action: 'add' | 'modify' | 'delete' | 'reorder'
  gateType?: string
  params?: Record<string, unknown>
  targetIndex?: number
}

interface DialogFlowProps {
  bridge: ReturnType<typeof createAIBridge>
  onCommitDone?: (success: boolean) => void
  onClose?: () => void
}

/* ── Semantic classifier keywords ──────────────────────────────────── */

const GATE_KEYWORDS = [
  '审批', '阈值', '检查', '超', '限制', '上限',
  'approve', 'threshold', 'check', 'limit',
  '拒绝', 'reject', '告警', 'warn', '阻止',
  '超限', '审核', '批准', '管控', '限额',
]

function detectGateIntent(text: string): boolean {
  const lower = text.toLowerCase()
  return GATE_KEYWORDS.some((k) => lower.includes(k))
}

/* ── DialogFlowController (exported as component) ──────────────────── */

export function DialogFlowController({ bridge, onCommitDone, onClose }: DialogFlowProps) {
  const [steps, setSteps] = useState<DialogStep[]>([
    { id: 'step_1', phase: 'field_define', prompt: '请输入您要配置的字段名（如 server_port）', userConfirm: false },
  ])
  const [currentStepIdx, setCurrentStepIdx] = useState(0)
  const [inputText, setInputText] = useState('')
  const [isProcessing, setIsProcessing] = useState(false)
  const [accumulatedSchema] = useState<Record<string, unknown>>({})
  const [accumulatedGates] = useState<GateChainEdit[]>([])
  const [flowDone, setFlowDone] = useState(false)
  const inputRef = useRef<HTMLInputElement>(null)

  const currentStep = steps[currentStepIdx]

  /* ── Suggest next step from AI via bridge ─────────────────────────── */
  const suggestNextStep = useCallback(async (userInput: string, phase: DialogPhase) => {
    const systemPrompt = `你是 BlessStar 配置编辑器的多轮对话助手。
当前阶段：${phase === 'field_define' ? '字段定义阶段 - 帮用户描述要配置的字段' : 'Gate 链编辑阶段 - 帮用户描述门禁规则'}
用户输入: "${userInput}"
请生成下一步的提示文本（约 20-60 字中文），引导用户补充信息。如果是 gate_edit 阶段且检测到 Gate 意图，引导用户选择 Gate 类型。`

    const req: AICompletionRequest = {
      messages: [
        { role: 'system', content: systemPrompt },
        { role: 'user', content: userInput },
      ],
    }
    const resp = await bridge.complete(req)
    return resp.message.content
  }, [bridge])

  /* ── Commit all accumulated changes ──────────────────────────────── */
  const handleCommit = useCallback(async () => {
    setIsProcessing(true)
    try {
      // Validate schema
      const validResult = await window.blessstar.validateConfig(JSON.stringify(accumulatedSchema))
      if (!validResult.valid) {
        alert(`Schema 校验失败: ${JSON.stringify(validResult.errors)}`)
        onCommitDone?.(false)
        return { success: false }
      }

      // Register schema fields
      for (const step of steps) {
        if (step.phase === 'field_define' && step.userConfirm && step.result) {
          const r = step.result as Record<string, unknown>
          const toolResult: ToolResult = await executeToolCall({
            id: `commit_${step.id}`,
            type: 'function',
            function: {
              name: 'create_schema_field',
              arguments: JSON.stringify(r),
            },
          })
          if (!toolResult.success) {
            console.error('create_schema_field failed:', toolResult.error)
          }
        }
      }

      // Submit gate chain edits
      for (const gate of accumulatedGates) {
        if (gate.action === 'add') {
          await executeToolCall({
            id: `gate_${Date.now()}`,
            type: 'function',
            function: {
              name: 'update_gate_rule',
              arguments: JSON.stringify({ gateChainJson: { type: gate.gateType, ...gate.params } }),
            },
          })
        } else if (gate.action === 'modify') {
          await executeToolCall({
            id: `gate_mod_${Date.now()}`,
            type: 'function',
            function: {
              name: 'update_gate_rule',
              arguments: JSON.stringify(gate),
            },
          })
        } else if (gate.action === 'delete') {
          await executeToolCall({
            id: `gate_del_${Date.now()}`,
            type: 'function',
            function: {
              name: 'update_gate_rule',
              arguments: JSON.stringify({ action: 'remove_rule', index: gate.targetIndex }),
            },
          })
        }
      }

      // Sync Blockly workspace
      await executeToolCall({
        id: `sync_${Date.now()}`,
        type: 'function',
        function: {
          name: 'sync_blockly_workspace',
          arguments: '{}',
        },
      })

      setFlowDone(true)
      onCommitDone?.(true)
      return { success: true }
    } catch (err) {
      console.error('Commit failed:', err)
      onCommitDone?.(false)
      return { success: false }
    } finally {
      setIsProcessing(false)
    }
  }, [accumulatedSchema, accumulatedGates, steps, onCommitDone])

  /* ── Handle user input per step ──────────────────────────────────── */
  const handleNext = useCallback(async () => {
    const text = inputText.trim()
    if (!text || isProcessing) return

    setIsProcessing(true)
    const step = steps[currentStepIdx]

    // Mark current step as confirmed
    const updatedSteps = [...steps]
    updatedSteps[currentStepIdx] = {
      ...step,
      userConfirm: true,
      userEdit: text,
      result: { raw: text, phase: step.phase },
    }

    // Detect gate intent for phase transition
    const hasGateIntent = detectGateIntent(text) || detectGateIntent(step.prompt)
    const nextPhase: DialogPhase = hasGateIntent ? 'gate_edit' : 'field_define'

    // Generate next prompt
    const nextStepId = `step_${steps.length + 1}`
    const nextPrompt = await suggestNextStep(text, step.phase)

    const nextStep: DialogStep = {
      id: nextStepId,
      phase: nextPhase,
      prompt: nextPrompt,
      userConfirm: false,
    }

    const allSteps = [...updatedSteps, nextStep]
    setSteps(allSteps)
    setCurrentStepIdx(allSteps.length - 1)
    setInputText('')
    setIsProcessing(false)
    inputRef.current?.focus()
  }, [inputText, isProcessing, steps, currentStepIdx, suggestNextStep])

  /* ── Handle key down ──────────────────────────────────────────────── */
  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      e.preventDefault()
      handleNext()
    }
  }, [handleNext])

  if (flowDone) {
    return (
      <div className="p-4 text-center text-surface-600 dark:text-surface-400">
        <p className="text-lg font-medium text-green-600 dark:text-green-400">✅ 多轮对话完成</p>
        <p className="mt-2 text-sm">所有字段和 Gate 规则已提交。</p>
      </div>
    )
  }

  if (!currentStep) return null

  return (
    <div className="flex flex-col gap-3 p-3 border-t border-surface-200 dark:border-surface-700">
      {/* Step indicator */}
      <div className="flex items-center gap-2 text-xs text-surface-500">
        <span className={`px-2 py-0.5 rounded ${currentStep.phase === 'field_define' ? 'bg-primary-100 text-primary-700 dark:bg-primary-900/40 dark:text-primary-300' : 'bg-surface-100 text-surface-600 dark:bg-surface-700 dark:text-surface-400'}`}>
          字段定义
        </span>
        {currentStep.phase === 'gate_edit' && (
          <span className="px-2 py-0.5 rounded bg-accent-100 text-accent-700 dark:bg-accent-900/40 dark:text-accent-300">
            Gate 编辑
          </span>
        )}
        <span className="ml-auto">步骤 {currentStepIdx + 1} / {steps.length}</span>
      </div>

      {/* AI prompt */}
      <div className="bg-surface-50 dark:bg-surface-800 rounded-lg px-3 py-2 text-sm text-surface-700 dark:text-surface-300">
        🤖 {currentStep.prompt}
      </div>

      {/* Previous step result */}
      {currentStep.userEdit && (
        <div className="text-xs text-surface-500 dark:text-surface-400 pl-2 border-l-2 border-surface-300 dark:border-surface-600">
          已确认: {currentStep.userEdit}
        </div>
      )}

      {/* Input */}
      <div className="flex gap-2">
        <input
          ref={inputRef}
          type="text"
          value={inputText}
          onChange={(e) => setInputText(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder={currentStep.phase === 'gate_edit' ? '描述 Gate 规则...' : '输入字段信息...'}
          className="flex-1 px-3 py-2 text-sm rounded border border-surface-300 dark:border-surface-600 bg-white dark:bg-surface-700 text-surface-900 dark:text-surface-100 placeholder-surface-400 focus:outline-none focus:ring-2 focus:ring-primary-400"
          disabled={isProcessing}
        />
        <button
          onClick={handleNext}
          disabled={!inputText.trim() || isProcessing}
          className="px-4 py-2 bg-primary-500 text-white text-sm rounded hover:bg-primary-600 disabled:opacity-40 transition-colors"
        >
          确认
        </button>
      </div>

      {/* Commit button */}
      <button
        onClick={handleCommit}
        disabled={isProcessing}
        className="w-full py-2 bg-green-600 text-white text-sm rounded hover:bg-green-700 disabled:opacity-40 transition-colors mt-1"
      >
        完成全部配置并提交
      </button>

      {/* Close */}
      {onClose && (
        <button
          onClick={onClose}
          className="text-xs text-surface-400 hover:text-surface-600 dark:hover:text-surface-300 self-end"
        >
          关闭
        </button>
      )}
    </div>
  )
}

export default DialogFlowController
