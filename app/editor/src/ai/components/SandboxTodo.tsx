import { useState, useMemo, useEffect, useRef } from 'react'
import type { PlanStep } from '../types'
import { findTool } from '../executor'
import { evaluatePreGates } from '../preGate'
import type { PreGateRule } from '../types'

// ── 暗色模式检测 ──────────────────────────────────────────────────────

function useIsDark(): boolean {
  const [dark, setDark] = useState(() =>
    typeof document !== 'undefined' ? document.documentElement.classList.contains('dark') : false,
  )
  useEffect(() => {
    const el = document.documentElement
    const obs = new MutationObserver(() => setDark(el.classList.contains('dark')))
    obs.observe(el, { attributes: true, attributeFilter: ['class'] })
    return () => obs.disconnect()
  }, [])
  return dark
}

// ── 类型 ──────────────────────────────────────────────────────────────

export interface ToolCard {
  callId?: string
  toolName: string
  args: Record<string, unknown>
  success: boolean
  outputLines: string[]
  duration?: number
  preGatePassed: boolean
  preGateError?: string
  dependsOn?: string
}

interface SandboxTodoProps {
  steps: PlanStep[]
  thinking?: string
  toolCards?: ToolCard[]
  /** 旧接口兼容：tool 消息内容文本（toolCards 未提供时使用） */
  toolResultContent?: string
  isLatest?: boolean
}

// ── 常量 ──────────────────────────────────────────────────────────────

const MAX_VISIBLE_LINES = 3

function firstPendingIndex(steps: PlanStep[]): number {
  return steps.findIndex((s) => !s.done)
}

/** 从 ToolCallRecord 匹配 toolCards 与 planStep 的 callId */
function matchCardForStep(step: PlanStep, cards?: ToolCard[]): ToolCard | undefined {
  if (!step.callId || !cards) return undefined
  return cards.find((c) => c.callId === step.callId)
}

export function SandboxTodo({ steps, thinking, toolCards, toolResultContent, isLatest }: SandboxTodoProps) {
  const [expanded, setExpanded] = useState(!!(toolCards && toolCards.length > 0))
  const [thinkingOpen, setThinkingOpen] = useState(false)
  const [sandboxOpen, setSandboxOpen] = useState(true)
  const isDark = useIsDark()

  // ── 折叠时预览：当前正在执行的步骤（带切换动画） ──
  // 有运行步骤时显示当前步骤；全部完成后显示最后一步 + ✅
  const runningIdx = isLatest ? firstPendingIndex(steps) : -1
  const runningText = runningIdx >= 0 && runningIdx < steps.length ? steps[runningIdx].text : null
  // 全部完成的兜底：取最后一个已完成步骤
  const allDoneFallback = (runningIdx === -1 && steps.length > 0)
    ? `✅ ${steps[steps.length - 1].text}`
    : null
  const previewText = runningText ?? allDoneFallback
  const [stepDisplay, setStepDisplay] = useState<string | null>(previewText)
  const [stepAnim, setStepAnim] = useState<'enter' | 'steady' | 'exit'>('steady')
  const prevRunningIdxRef = useRef(runningIdx)

  useEffect(() => {
    if (runningIdx === prevRunningIdxRef.current) return
    prevRunningIdxRef.current = runningIdx
    const nextText = runningIdx >= 0 && runningIdx < steps.length ? steps[runningIdx].text : null
    const nextFallback = !nextText && steps.length > 0 ? `✅ ${steps[steps.length - 1].text}` : null
    const target = nextText ?? nextFallback
    if (!stepDisplay && !target) return  // 初始无运行步骤，无需动画
    setStepAnim('exit')
    const t1 = setTimeout(() => {
      setStepDisplay(target)
      setStepAnim('enter')
      const t2 = setTimeout(() => setStepAnim('steady'), 400)
      return () => clearTimeout(t2)
    }, 180)
    return () => clearTimeout(t1)
  }, [runningIdx, steps])
  // ── end step tracker ──

  // ── Liquid Glass 水滴透明玻璃 + 独立色板（edge-only，无内部反光） ──
  // 核心规则：不用 inset box-shadow（会向内部渐变），用纯 border + 仅外阴影
  // 色板纯色（不做对角渐变），边缘高光通过多色 border 实现（左上亮、右下暗）
  const palette = {
    solidBg: isDark
      ? 'rgba(245,247,250,0.70)'
      : 'rgba(30,41,59,0.70)',
    blur: 'blur(10px) saturate(160%)',
    // 多色 border：左上高光边（模拟光线折射）+ 右下暗边（不显眼）
    borderTop: isDark ? 'rgba(255,255,255,0.50)' : 'rgba(255,255,255,0.22)',
    borderLeft: isDark ? 'rgba(255,255,255,0.45)' : 'rgba(255,255,255,0.20)',
    borderRight: isDark ? 'rgba(200,205,215,0.30)' : 'rgba(255,255,255,0.08)',
    borderBottom: isDark ? 'rgba(180,185,195,0.30)' : 'rgba(255,255,255,0.06)',
    // 三层悬浮阴影：contact → penumbra → ambient（模拟物理漂浮）
    // 玻璃材质用较宽较软的阴影 + 负 spread 收窄接触阴影
    shadow: isDark
      ? '0 2px 4px rgba(0,0,0,0.12), 0 8px 20px rgba(0,0,0,0.10), 0 28px 48px rgba(0,0,0,0.06)'
      : '0 2px 4px rgba(0,0,0,0.08), 0 8px 20px rgba(0,0,0,0.06), 0 28px 48px rgba(0,0,0,0.04)',
    headerBg: 'transparent',
    headerBorder: isDark
      ? 'rgba(0, 0, 0, 0.10)'
      : 'rgba(255, 255, 255, 0.14)',
    thinkBodyBg: 'transparent',
    muted: isDark ? '#334155' : '#CBD5E1',
    text: isDark ? '#000000' : '#FFFFFF',
    textShadow: isDark ? '0 1px 2px rgba(0,0,0,0.15)' : '0 1px 2px rgba(0,0,0,0.30)',
    blue: isDark ? '#1D4ED8' : '#93C5FD',
    amber: isDark ? '#B45309' : '#FCD34D',
    emerald: isDark ? '#047857' : '#6EE7B7',
    purple: isDark ? '#6D28D9' : '#C4B5FD',
  }

  // ── 构建显示行 ──
  const displayRows = useMemo(() => {
    const rows: Array<{ type: 'step' | 'detail'; content: string; color: string; key: string; suffix?: string }> = []
    const pendingIdx = firstPendingIndex(steps)

    for (let i = 0; i < steps.length; i++) {
      const step = steps[i]
      const isPending = !step.done
      const isRunning = isPending && i === pendingIdx && isLatest
      const card = matchCardForStep(step, toolCards)

      if (isRunning) {
        rows.push({ type: 'step', content: `💭 ${step.text}`, color: 'text-blue-400', key: `s${i}` })
        if (thinking) {
          rows.push({ type: 'detail', content: `     ${thinking}`, color: '', key: `s${i}-think` })
        }
      } else if (isPending) {
        rows.push({ type: 'step', content: `🟪 ${step.text}`, color: 'text-surface-500', key: `s${i}` })
      } else {
        // 已完成
        const evStatus = step.evidenceStatus
        let icon = '✅'
        if (evStatus === 'unmatched') icon = '🚫'
        else if (evStatus === 'matched' || card) icon = '✅'

        const detailParts: string[] = []
        if (step.toolName) detailParts.push(step.toolName)
        if (card?.duration) detailParts.push(`${card.duration}ms`)
        const extra = detailParts.length > 0 ? ` · ${detailParts.join(' · ')}` : ''

        rows.push({
          type: 'step',
          content: `${icon} ${step.text}`,
          color: evStatus === 'unmatched' ? 'text-red-400' : 'text-green-400',
          key: `s${i}`,
          suffix: extra || undefined,
        })

        // 展开模式下显示 tool call + return 详情
        if (card && card.outputLines.length > 0) {
          for (const line of card.outputLines) {
            rows.push({ type: 'detail', content: `  ${line}`, color: '', key: `s${i}-d${line.slice(0, 8)}` })
          }
        }

        // preGate 校验失败标记
        if (card && card.preGateError) {
          rows.push({ type: 'detail', content: `  ⚠️ 参数校验: ${card.preGateError}`, color: 'text-red-400', key: `s${i}-pg` })
        }

        // 证据不足标记
        if (evStatus === 'unmatched') {
          rows.push({ type: 'detail', content: `  🚫 证据不足：无 Registry 执行记录`, color: 'text-red-400', key: `s${i}-ev` })
        }
      }
    }

    return rows
  }, [steps, thinking, toolCards, toolResultContent, isLatest])

  // ── toolResultContent 兜底：toolCards 未提供时，将文本追加到已完成的步骤 ──
  const allRows = useMemo(() => {
    if (toolCards && toolCards.length > 0) return displayRows
    if (!toolResultContent) return displayRows

    // 寻找最后一个完成的步骤，追加 detail 行
    const rows = [...displayRows]
    const lines = toolResultContent.split('\n').filter(Boolean)
    for (const line of lines) {
      rows.push({ type: 'detail', content: `  ${line}`, color: '', key: `trcfb-${line.slice(0, 8)}` })
    }
    return rows
  }, [displayRows, toolCards, toolResultContent])

  const visibleRows = expanded ? allRows : allRows.slice(0, MAX_VISIBLE_LINES)
  const hasMore = allRows.length > MAX_VISIBLE_LINES
  const doneCount = steps.filter((s) => s.done).length

  return (
    <div className="max-w-[85%] rounded-2xl text-sm font-mono font-bold relative overflow-hidden break-all"
      style={{
        background: palette.solidBg,
        backdropFilter: palette.blur,
        WebkitBackdropFilter: palette.blur,
        borderTop: `1px solid ${palette.borderTop}`,
        borderLeft: `1px solid ${palette.borderLeft}`,
        borderRight: `1px solid ${palette.borderRight}`,
        borderBottom: `1px solid ${palette.borderBottom}`,
        boxShadow: palette.shadow,
        color: palette.text,
        textShadow: palette.textShadow,
        wordBreak: 'break-all',
        overflowWrap: 'break-word',
      }}
    >
      {/* 内容包装层 — 内部纯净，无额外反光层 */}
      <div className="relative z-[1]">
      {/* ── Collapsible thinking header ── */}
      {thinking && (
        <>
          <div
            className="flex items-center gap-1.5 px-3 py-1.5 cursor-pointer select-none transition-all duration-200 hover:brightness-125"
            onClick={() => setThinkingOpen(!thinkingOpen)}
            style={{
              background: palette.headerBg,
              borderBottom: `1px solid ${palette.headerBorder}`,
            }}
          >
            <svg
              className={`w-3 h-3 transition-transform ${thinkingOpen ? 'rotate-90' : ''}`}
              viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth={2}
              style={{ color: palette.muted }}
            >
              <path d="M9 18l6-6-6-6" />
            </svg>
            <span className="text-[10px] uppercase tracking-wider font-medium" style={{ color: palette.muted }}>思考过程</span>
            <span className="text-[10px] ml-1" style={{ color: palette.muted }}>({steps.length} 步)</span>
            {!thinkingOpen && stepDisplay && (
              <span
                className="text-[10px] ml-auto truncate max-w-[240px]"
                style={{
                  color: palette.blue,
                  opacity: stepAnim === 'exit' ? 0 : 1,
                  transform: stepAnim === 'exit' ? 'translateY(-3px)' : 'translateY(0)',
                  transition: stepAnim === 'exit' ? 'all 150ms ease-in' : 'all 350ms ease-out',
                }}
              >
                {stepDisplay}
              </span>
            )}
          </div>
          {thinkingOpen && (
            <div
              className="px-3 py-2 text-[10px]"
              style={{
                background: palette.thinkBodyBg,
                borderBottom: `1px solid ${palette.headerBorder}`,
              }}
            >
              {thinking.split('\n').map((line, i) => {
                const isStepLine = /^\d+，/.test(line)
                const isList = line.includes('[LIST]')
                const isWrite = line.includes('[WRITE]')
                const isRead = line.includes('[READ]')
                const isChat = line.includes('[CHAT]')
                // 自然语言行（用户描述、分析等）用纯白/纯黑；
                // 步骤行（1，[LIST]...）用操作色区分解读意图
                const opClr = isStepLine
                  ? isList ? palette.blue : isWrite ? palette.amber : isRead ? palette.emerald : isChat ? palette.purple : palette.text
                  : palette.text
                return (
                  <div key={`think-${i}`} className="leading-5 break-all whitespace-normal" style={{ color: opClr }}>
                    {line}
                  </div>
                )
              })}
            </div>
          )}
        </>
      )}

      {/* ── Sandbox body: tool cards + progress ── */}
      <div>
        {/* Sandbox header - clickable to collapse/expand */}
        <div
          className="flex items-center justify-between px-3 py-1.5 cursor-pointer select-none transition-all duration-200 hover:brightness-110"
          onClick={() => { const next = !sandboxOpen; setSandboxOpen(next); if (!next) setExpanded(false) }}
          style={{
            background: palette.headerBg,
            borderBottom: sandboxOpen ? `1px solid ${palette.headerBorder}` : 'none',
          }}
        >
          <div className="flex items-center gap-1.5">
            <svg
              className={`w-3 h-3 transition-transform ${sandboxOpen ? 'rotate-90' : ''}`}
              viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth={2}
              style={{ color: palette.muted }}
            >
              <path d="M9 18l6-6-6-6" />
            </svg>
            <svg className="w-3.5 h-3.5" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth={2} style={{ color: palette.muted }}>
              <rect x="3" y="3" width="18" height="18" rx="2" />
              <path d="M3 9h18" />
              <path d="M9 21V9" />
            </svg>
            <span className="text-[10px] uppercase tracking-wider font-medium" style={{ color: palette.muted }}>任务沙箱</span>
          </div>
          <span className="text-[10px] font-medium" style={{ color: palette.muted }}>{doneCount}/{steps.length}</span>
        </div>

        {sandboxOpen && (
        <>
        {/* Step rows */}
        <div className="px-3 py-2 space-y-0.5">
          {visibleRows.map((row) => (
            <div key={row.key} className={`leading-5 break-all whitespace-normal ${row.color}`}>
              {row.content}
              {row.suffix ? <span className="whitespace-nowrap">{row.suffix}</span> : null}
            </div>
          ))}

          {/* 展开/折叠 */}
          {hasMore && (
            <button
              onClick={(e) => { e.stopPropagation(); setExpanded(!expanded) }}
              className="mt-1 text-[10px] hover:underline transition-colors cursor-pointer select-none flex items-center gap-1"
              style={{ color: palette.blue }}
            >
              {expanded ? '▲ 收起' : `▼ 展开（共 ${allRows.length} 行）`}
            </button>
          )}
        </div>
        </>
        )}
      </div>
      </div>{/* close content wrapper */}
    </div>
  )
}

// ── Helper：从 tool 执行结果构建 ToolCard ────────────────────────────

/**
 * 从 tool call 执行结果构建声明式 ToolCard。
 * 优先使用 toolFactory 的 resultRenderer；未迁移工具回退到原始文本行。
 */
export function buildToolCard(
  callId: string,
  toolName: string,
  args: Record<string, unknown>,
  success: boolean,
  resultData: unknown,
  duration?: number,
): ToolCard {
  // preGate 校验
  const tool = findTool(toolName) as Record<string, unknown> | undefined
  const preGateRules = tool?.preGates as PreGateRule[] | undefined
  let preGatePassed = true
  let preGateError: string | undefined

  if (preGateRules) {
    const err = evaluatePreGates(preGateRules, args)
    if (err) {
      preGatePassed = false
      preGateError = err
    }
  }

  // 输出行
  let outputLines: string[]
  if (success && tool?.resultRenderer && typeof tool.resultRenderer === 'function') {
    outputLines = (tool.resultRenderer as (d: unknown) => string[])(resultData)
  } else if (!success) {
    const errorMsg = (resultData as Record<string, unknown>)?.error || '未知错误'
    outputLines = [`❌ ${String(errorMsg)}`]
  } else {
    // 回退：原始文本
    const data = resultData as Record<string, unknown> | undefined
    if (data?.written) {
      const w = data.written as Record<string, string>
      outputLines = [`已将 ${w.key} 修改为 ${w.value}`, `return: ${w.key} = ${w.value}`]
    } else if (data?.value !== undefined) {
      outputLines = [`return: ${String(data.key)} = ${String(data.value)}`]
    } else if (data?.message) {
      outputLines = [`return: ${String(data.message)}`]
    } else {
      outputLines = ['return: ✅ 成功']
    }
  }

  return { callId, toolName, args, success, outputLines, duration, preGatePassed, preGateError }
}
