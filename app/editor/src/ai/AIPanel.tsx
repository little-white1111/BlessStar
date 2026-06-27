/**
 * AIPanel — AI 助手面板（纯 UI 组件）
 *
 * 重构后职责：UI 渲染 + 状态管理 + 事件处理。
 * AI 管线逻辑已抽离至 pipeline/pipelineManager.ts。
 */

import { useState, useRef, useEffect, useCallback } from 'react'
import type { AIMessage, PlanStep } from './types'
import { createAIBridge } from './bridge'
import { executePipeline } from './pipeline/pipelineManager'
import { FeedbackCollector } from './context-manager/feedbackCollector'
import { loadDefaultParadigms } from './context-manager/paradigm'
import { SandboxTodo } from './components/SandboxTodo'
import { AskSelector } from './components/AskSelector'
import type { AskCandidate } from './components/AskSelector'
import { LABEL_TO_KEY } from './tools/configLabels'
import { confirmMatch } from './pipeline/retriever'
import { clearEmbeddingCache, initEmbeddingCache, getAllDomains } from './context-manager/indexShardLoader'
import { UNIFIED_SKILLS } from './context-manager/skillRouter'

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

export type PanelPosition = 'right' | 'bottom'

interface AIPanelProps {
  isOpen: boolean
  onClose: () => void
  initialPosition?: PanelPosition
  onAcceptSuggestion?: (suggestion: string) => void
  position?: PanelPosition
  panelSize?: number
  onResize?: (size: number) => void
  onChangePosition?: (pos: PanelPosition) => void
}

export default function AIPanel({
  isOpen, onClose, initialPosition = 'right', onAcceptSuggestion,
  position: extPosition, onChangePosition,
  panelSize: extPanelSize,
  onResize: extOnResize,
}: AIPanelProps) {
  const [position, setPosition] = useState<PanelPosition>(initialPosition)
  const effectivePosition = extPosition ?? position
  const setEffectivePosition = onChangePosition ?? setPosition

  const [internalSize, setInternalSize] = useState<number>(() => {
    const saved = localStorage.getItem('ai_panel_size')
    if (saved) {
      const n = Number(saved)
      if (!Number.isNaN(n)) return n
    }
    return effectivePosition === 'right' ? 420 : 320
  })
  const panelSize = extPanelSize ?? internalSize
  const setPanelSize = useCallback((size: number) => {
    if (extOnResize) extOnResize(size)
    if (!extPanelSize) {
      setInternalSize(size)
      localStorage.setItem('ai_panel_size', String(size))
    }
  }, [extOnResize, extPanelSize])

  const [messages, setMessages] = useState<AIMessage[]>([
    {
      role: 'assistant',
      content: '您好！我是 AI 配置助手，可以帮您创建 Schema 字段、配置 Gate 规则、校验配置等。请问需要什么帮助？',
    },
  ])
  const [inputText, setInputText] = useState('')
  const [isProcessing, setIsProcessing] = useState(false)
  const [askLoading, setAskLoading] = useState(false)
  const [currentSuggestion, setCurrentSuggestion] = useState<string | null>(null)
  const [provider, setProvider] = useState<'ollama' | 'deepseek' | 'openai'>(() => {
    return (localStorage.getItem('ai_provider') as 'ollama' | 'deepseek' | 'openai') || 'ollama'
  })
  const [apiKey, setApiKey] = useState(() => localStorage.getItem('ai_api_key') || '')
  const [ollamaModel, setOllamaModel] = useState(() => localStorage.getItem('ai_ollama_model') || 'qwen2.5-coder:7b')
  const [showSettings, setShowSettings] = useState(false)
  const [isResizing, setIsResizing] = useState(false)

  // ── 命令提示（/ 前缀自动补全）─────────────────────────────────────
  const [cmdSuggestions, setCmdSuggestions] = useState<Array<{ command: string; description: string; intent: string }>>([])
  const [selectedCmdIndex, setSelectedCmdIndex] = useState(0)
  const [showCmdSuggestions, setShowCmdSuggestions] = useState(false)

  const messagesEndRef = useRef<HTMLDivElement>(null)
  const inputRef = useRef<HTMLTextAreaElement>(null)
  const bridgeRef = useRef(createAIBridge(
    (() => {
      const savedProvider = localStorage.getItem('ai_provider') as 'ollama' | 'deepseek' | 'openai' | null
      const savedKey = localStorage.getItem('ai_api_key') || ''
      const savedModel = localStorage.getItem('ai_ollama_model') || 'qwen2.5-coder:7b'
      if (savedProvider === 'ollama' || !savedProvider) {
        return { provider: 'ollama' as const, ollamaUrl: 'http://localhost:11434', ollamaModel: savedModel }
      }
      return { provider: savedProvider, apiKey: savedKey }
    })(),
  ))

  const lastToolDeltaRef = useRef<ReturnType<typeof import('./context-manager/toolDeltaFormatter').buildToolDelta> | undefined>(undefined)
  const feedbackRef = useRef(new FeedbackCollector())
  const loadParadigmsDone = useRef(false)
  // D38-4-INV-04: ASK 管线挂起状态（跨 handleSend 持久化）
  const suspendedStateRef = useRef<{
    question: string
    candidates: Array<{ label: string; configKey: string; aiHint: string }>
    originalSubject: string
    originalUserInput: string
    fallbackMessage: string
    intent: string
    subject: string
    value: string | null
    planSteps: PlanStep[]
    sessionState?: import('./pipeline/types').SessionState | null
  } | null>(null)
  const awaitingConfirmationRef = useRef(false)

  // ── 版本回退：记录每条用户消息对应的 WRITE 条目 + versionIds（第33天 · RV-04/06）─
  const messageWritesRef = useRef<Map<number, {
    writes: Array<{ key: string; value: string }>
    versionIds: Record<string, string>
  }>>(new Map())
  const [rollbackMenu, setRollbackMenu] = useState<{ messageIndex: number; open: boolean; subKey?: string }>({ messageIndex: -1, open: false })
  const [rollbackVersions, setRollbackVersions] = useState<import('./types').VersionRegistry>({})
  const rollbackBtnRef = useRef<Map<number, HTMLButtonElement | null>>(new Map())
  const isDark = useIsDark()

  useEffect(() => {
    if (!loadParadigmsDone.current) {
      loadDefaultParadigms()
      loadParadigmsDone.current = true
    }
    // EMB: 启动时预计算 domain embedding 向量
    const bridge = bridgeRef.current
    initEmbeddingCache((text: string) => bridge.embed(text), getAllDomains()).catch(() => {
      // embedding 预计算失败不阻塞 UI
    })
  }, [])

  const scrollToBottom = useCallback(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [])

  useEffect(() => { scrollToBottom() }, [messages, scrollToBottom])
  useEffect(() => { if (isOpen) inputRef.current?.focus() }, [isOpen])

  // ── Resize drag handler ──
  useEffect(() => {
    if (!isResizing) return
    const onMove = (e: MouseEvent) => {
      if (effectivePosition === 'right') {
        // Dragging left edge — width is (window.width - mouse.x)
        const newWidth = Math.max(300, Math.min(window.innerWidth * 0.7, window.innerWidth - e.clientX))
        setPanelSize(Math.round(newWidth))
      } else {
        // Dragging top edge — height is (window.height - mouse.y)
        const newHeight = Math.max(200, Math.min(window.innerHeight * 0.7, window.innerHeight - e.clientY))
        setPanelSize(Math.round(newHeight))
      }
    }
    const onUp = () => setIsResizing(false)
    window.addEventListener('mousemove', onMove)
    window.addEventListener('mouseup', onUp)
    const prevUserSelect = document.body.style.userSelect
    document.body.style.userSelect = 'none'
    return () => {
      window.removeEventListener('mousemove', onMove)
      window.removeEventListener('mouseup', onUp)
      document.body.style.userSelect = prevUserSelect
    }
  }, [isResizing, effectivePosition, setPanelSize])

  // ── 连接管理 ──
  const reconnectBridge = useCallback((p: string, key: string, model: string) => {
    const prov = p as 'ollama' | 'deepseek' | 'openai'
    bridgeRef.current = createAIBridge(prov === 'ollama'
      ? { provider: 'ollama', ollamaUrl: 'http://localhost:11434', ollamaModel: model || 'qwen2.5-coder:7b' }
      : { provider: prov, apiKey: key, model: prov === 'deepseek' ? 'deepseek-chat' : 'gpt-4o' },
    )
    localStorage.setItem('ai_provider', prov)
    localStorage.setItem('ai_api_key', key)
    localStorage.setItem('ai_ollama_model', model)
    // EMB: 模型切换 → 清空 embedding 缓存并重算
    clearEmbeddingCache()
    const bridge = bridgeRef.current
    initEmbeddingCache((text: string) => bridge.embed(text), getAllDomains()).catch(() => {
      // embedding 预计算失败不阻塞 UI
    })
  }, [])

  // ── 核心发送 ──
  const handleSend = useCallback(async () => {
    const text = inputText.trim()
    if (!text || isProcessing) return

    // D38-4-INV-04: ASK 挂起状态 → 忽略键盘输入，由 AskSelector 处理
    if (awaitingConfirmationRef.current && suspendedStateRef.current) {
      return
    }

    setInputText('')
    setCurrentSuggestion(null)
    setMessages(prev => [...prev, { role: 'user', content: text }])
    setIsProcessing(true)

    try {
      await executePipeline(text, {
        appendMessage: (msg) => setMessages(prev => [...prev, msg]),
        updateLastAssistant: (updater) => setMessages(prev => {
          const idx = [...prev].reverse().findIndex(m => m.role === 'assistant')
          if (idx < 0) return prev
          const realIdx = prev.length - 1 - idx
          return prev.map((m, i) => i === realIdx ? updater(m) : m)
        }),
        setSuggestion: (data: string | null) => {
          // Only set suggestion if it looks like actionable code / JSON (not just informational output)
          if (!data) { setCurrentSuggestion(null); return }
          const trimmed = data.trim()
          if (trimmed.length < 20) { setCurrentSuggestion(null); return }
          // Heuristic: only accept JSON objects / arrays or multi-line code
          const looksLikeCode =
            trimmed.startsWith('{') || trimmed.startsWith('[') ||
            trimmed.includes('\n') || trimmed.includes('"field"') ||
            trimmed.includes('"name"') || trimmed.includes('"schema"') ||
            trimmed.includes('"gate"')
          if (looksLikeCode) setCurrentSuggestion(data)
          else setCurrentSuggestion(null)
        },
        setProcessing: setIsProcessing,
        feedbackRef,
        lastToolDeltaRef,
        getBridge: () => bridgeRef.current,
        getMessages: () => messages,
        onWriteEntries: (writes, newVersionIds) => {
          // 记录本轮用户消息（最新一条 user 消息）的 WRITE 条目 + versionIds
          const idx = messages.length
          messageWritesRef.current.set(idx, { writes: writes.slice(), versionIds: { ...newVersionIds } })
        },
        onSuspend: (newState) => {
          suspendedStateRef.current = newState
          awaitingConfirmationRef.current = true
        },
      })
    } catch (err) {
      const errMsg = err instanceof Error ? err.message : '未知错误'
      setMessages(prev => [...prev, { role: 'assistant', content: `处理请求时出错：${errMsg}` }])
      setIsProcessing(false)
    }
  }, [inputText, isProcessing, messages])

  // ── D38-4-INV-04: ASK 选择回调 ─────────────────────────────────
  const handleAskSelect = useCallback(async (candidate: AskCandidate | null) => {
    const state = suspendedStateRef.current
    if (!state) return

    if (candidate) {
      // 用户选了某个候选

      // D38-4-OPT-B: 自定义查询回滚 loop → 咨询 agent 判断可行性
      if (candidate.configKey.startsWith('__custom_query__:')) {
        const customInput = candidate.configKey.replace('__custom_query__:', '')
        awaitingConfirmationRef.current = false
        suspendedStateRef.current = null
        setAskLoading(true)
        try {
          const feasibility = await bridgeRef.current.complete({
            messages: [
              {
                role: 'system',
                content: `你是 BlessStar 配置系统的咨询顾问。用户输入了一段描述，请判断当前系统是否能通过已有工具（读取配置、写入配置、列出配置、列出目录、搜索内容、查找文件、读取文件、执行终端命令、读取诊断、生成模板、对话、执行查询、验证配置、更新规则、创建字段等）解决。

如果能解决，请仅回复一个词：FEASIBLE
如果不能解决，请回复：INFEASIBLE:原因说明

规则：
- 只有完全无法对应任何工具时才返回 INFEASIBLE
- 读取目录内容、搜索文件等都属于可解决范围
- 不要擅自假设新功能`
              },
              { role: 'user', content: customInput },
            ],
            temperature: 0.1,
          })
          const reply = (feasibility?.message?.content || '').trim()
          if (reply.startsWith('FEASIBLE')) {
            // 可行 → 用新描述重新执行管线
            await executePipeline(customInput, {
              appendMessage: (msg) => setMessages(prev => [...prev, msg]),
              updateLastAssistant: (updater) => setMessages(prev => {
                const idx = [...prev].reverse().findIndex(m => m.role === 'assistant')
                if (idx < 0) return prev
                const realIdx = prev.length - 1 - idx
                return prev.map((m, i) => i === realIdx ? updater(m) : m)
              }),
              setSuggestion: (data) => {
                if (!data) { setCurrentSuggestion(null); return }
                const trimmed = data.trim()
                if (trimmed.length < 20) { setCurrentSuggestion(null); return }
                const looksLikeCode =
                  trimmed.startsWith('{') || trimmed.startsWith('[') ||
                  trimmed.includes('\n') || trimmed.includes('"field"') ||
                  trimmed.includes('"name"') || trimmed.includes('"schema"') ||
                  trimmed.includes('"gate"')
                if (looksLikeCode) setCurrentSuggestion(data)
                else setCurrentSuggestion(null)
              },
              setProcessing: setIsProcessing,
              feedbackRef,
              lastToolDeltaRef,
              getBridge: () => bridgeRef.current,
              getMessages: () => messages,
              onWriteEntries: (writes, newVersionIds) => {
                const idx = messages.length
                messageWritesRef.current.set(idx, { writes: writes.slice(), versionIds: { ...newVersionIds } })
              },
              onSuspend: (newState) => {
                suspendedStateRef.current = newState
                awaitingConfirmationRef.current = true
              },
              sessionState: state.sessionState,
            })
          } else {
            // 不可行 → 展示原因
            const reason = reply.replace(/^INFEASIBLE:?\s*/i, '')
            setMessages(prev => [...prev, { role: 'assistant', content: `「${customInput}」当前暂不支持该功能。${reason ? '\n' + reason : ''}` }])
            setIsProcessing(false)
          }
        } catch (err) {
          const errMsg = err instanceof Error ? err.message : '未知错误'
          setMessages(prev => [...prev, { role: 'assistant', content: `处理自定义查询时出错：${errMsg}` }])
          setIsProcessing(false)
        }
        return
      }

      // D38-4-OPT-A: 路径字段为空 → 用户已在 AskSelector 中输入值
      if (state.intent === '__path_empty' && candidate.configKey.startsWith('__input__:')) {
        const inputPath = candidate.configKey.replace('__input__:', '')
        const configKey = state.originalSubject
        awaitingConfirmationRef.current = false
        suspendedStateRef.current = null
        setAskLoading(true)
        try {
          await window.blessstar.executeTool('write_config_value', { key: configKey, value: inputPath })
        } catch (err) {
          // 写值失败不阻断
        }
        try {
          await executePipeline(state.originalUserInput, {
            appendMessage: (msg) => setMessages(prev => [...prev, msg]),
            updateLastAssistant: (updater) => setMessages(prev => {
              const idx = [...prev].reverse().findIndex(m => m.role === 'assistant')
              if (idx < 0) return prev
              const realIdx = prev.length - 1 - idx
              return prev.map((m, i) => i === realIdx ? updater(m) : m)
            }),
            setSuggestion: (data) => {
              if (!data) { setCurrentSuggestion(null); return }
              const trimmed = data.trim()
              if (trimmed.length < 20) { setCurrentSuggestion(null); return }
              const looksLikeCode =
                trimmed.startsWith('{') || trimmed.startsWith('[') ||
                trimmed.includes('\n') || trimmed.includes('"field"') ||
                trimmed.includes('"name"') || trimmed.includes('"schema"') ||
                trimmed.includes('"gate"')
              if (looksLikeCode) setCurrentSuggestion(data)
              else setCurrentSuggestion(null)
            },
            setProcessing: setIsProcessing,
            feedbackRef,
            lastToolDeltaRef,
            getBridge: () => bridgeRef.current,
            getMessages: () => messages,
            onWriteEntries: (writes, newVersionIds) => {
              const idx = messages.length
              messageWritesRef.current.set(idx, { writes: writes.slice(), versionIds: { ...newVersionIds } })
            },
            onSuspend: (newState) => {
              suspendedStateRef.current = newState
              awaitingConfirmationRef.current = true
            },
            sessionState: state.sessionState,
          })
        } catch (err) {
          const errMsg = err instanceof Error ? err.message : '未知错误'
          setMessages(prev => [...prev, { role: 'assistant', content: `处理请求时出错：${errMsg}` }])
          setIsProcessing(false)
        }
        return
      }

      // D38-4-OPT-A: 路径为空 → 用户选 B（中止），展示取消消息
      if (state.intent === '__path_empty') {
        awaitingConfirmationRef.current = false
        suspendedStateRef.current = null
        setMessages(prev => [...prev, { role: 'assistant', content: state.fallbackMessage || '已取消查询。' }])
        return
      }

      // 普通 ASK：注册别名，重新执行原始输入
      awaitingConfirmationRef.current = false
      suspendedStateRef.current = null
      setAskLoading(true)

      const targetKey = candidate.configKey
      confirmMatch(state.originalSubject, targetKey)

      try {
        await executePipeline(state.originalUserInput, {
          appendMessage: (msg) => setMessages(prev => [...prev, msg]),
          updateLastAssistant: (updater) => setMessages(prev => {
            const idx = [...prev].reverse().findIndex(m => m.role === 'assistant')
            if (idx < 0) return prev
            const realIdx = prev.length - 1 - idx
            return prev.map((m, i) => i === realIdx ? updater(m) : m)
          }),
          setSuggestion: (data) => {
            if (!data) { setCurrentSuggestion(null); return }
            const trimmed = data.trim()
            if (trimmed.length < 20) { setCurrentSuggestion(null); return }
            const looksLikeCode =
              trimmed.startsWith('{') || trimmed.startsWith('[') ||
              trimmed.includes('\n') || trimmed.includes('"field"') ||
              trimmed.includes('"name"') || trimmed.includes('"schema"') ||
              trimmed.includes('"gate"')
            if (looksLikeCode) setCurrentSuggestion(data)
            else setCurrentSuggestion(null)
          },
          setProcessing: setIsProcessing,
          feedbackRef,
          lastToolDeltaRef,
          getBridge: () => bridgeRef.current,
          getMessages: () => messages,
          onWriteEntries: (writes, newVersionIds) => {
            const idx = messages.length
            messageWritesRef.current.set(idx, { writes: writes.slice(), versionIds: { ...newVersionIds } })
          },
          onSuspend: (newState) => {
            suspendedStateRef.current = newState
            awaitingConfirmationRef.current = true
          },
          sessionState: state.sessionState,
        })
      } catch (err) {
        const errMsg = err instanceof Error ? err.message : '未知错误'
        setMessages(prev => [...prev, { role: 'assistant', content: `处理请求时出错：${errMsg}` }])
        setIsProcessing(false)
      }
    } else {
      // 用户选了"全都不是"
      awaitingConfirmationRef.current = false
      suspendedStateRef.current = null
      setMessages(prev => [...prev, { role: 'assistant', content: state.fallbackMessage }])
    }
    setAskLoading(false)
  }, [])

  // ── 命令提示过滤逻辑 ─────────────────────────────────────────────
  const filterCommands = useCallback((text: string) => {
    if (!text.startsWith('/')) {
      setShowCmdSuggestions(false)
      return
    }
    const prefix = text.toLowerCase()
    const allCommands = UNIFIED_SKILLS.flatMap(s =>
      s.triggers.exactCommands.map(cmd => ({
        command: cmd,
        description: s.description,
        intent: s.intent,
      }))
    )
    const filtered = allCommands.filter(c => c.command.toLowerCase().startsWith(prefix))
    if (filtered.length > 0) {
      setCmdSuggestions(filtered)
      setSelectedCmdIndex(0)
      setShowCmdSuggestions(true)
    } else {
      setShowCmdSuggestions(false)
    }
  }, [])

  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    // 命令提示键盘导航
    if (showCmdSuggestions && cmdSuggestions.length > 0) {
      if (e.key === 'ArrowDown') {
        e.preventDefault()
        setSelectedCmdIndex(i => (i + 1) % cmdSuggestions.length)
        return
      }
      if (e.key === 'ArrowUp') {
        e.preventDefault()
        setSelectedCmdIndex(i => (i - 1 + cmdSuggestions.length) % cmdSuggestions.length)
        return
      }
      if (e.key === 'Enter' || e.key === 'Tab') {
        e.preventDefault()
        const sel = cmdSuggestions[selectedCmdIndex]
        if (sel) {
          setInputText(sel.command + ' ')
          setShowCmdSuggestions(false)
          // 保持焦点
          setTimeout(() => inputRef.current?.focus(), 0)
        }
        return
      }
      if (e.key === 'Escape') {
        setShowCmdSuggestions(false)
        return
      }
    }
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      handleSend()
    }
  }, [handleSend, showCmdSuggestions, cmdSuggestions, selectedCmdIndex])

  const handleAcceptSuggestion = useCallback(() => {
    if (currentSuggestion && onAcceptSuggestion) {
      onAcceptSuggestion(currentSuggestion)
      setCurrentSuggestion(null)
      setMessages(prev => [...prev, {
        role: 'assistant',
        content: '✅ 建议已采纳，将应用到当前编辑器中。',
      }])
    }
  }, [currentSuggestion, onAcceptSuggestion])

  const handleClear = useCallback(() => {
    setMessages([
      { role: 'assistant', content: '对话已重置。请问需要什么帮助？' },
    ])
    setCurrentSuggestion(null)
    lastToolDeltaRef.current = undefined
    messageWritesRef.current.clear()
  }, [])

  // ── 版本回退（第33天 · RV-04/06）───────────────────────────────────
  const handleRollback = useCallback(async (
    messageIndex: number,
    entryKey?: string,
    targetVersionId?: string,
  ) => {
    const record = messageWritesRef.current.get(messageIndex)
    if (!record || record.writes.length === 0) return

    // 确定要回退的条目
    const targets = entryKey
      ? record.writes.filter(w => w.key === entryKey)
      : record.writes

    // 找出回退目标消息的用户原文
    const targetMsg = messages[messageIndex]
    const rollbackText = targetMsg?.content || ''

    if (targets.length === 0) {
      setRollbackMenu({ messageIndex: -1, open: false })
      return
    }

    // 加载版本注册表
    const { loadVersionRegistry, saveVersionRegistry } = await import('./versionRegistry')
    const registry = await loadVersionRegistry()

    for (const t of targets) {
      const versions = registry[t.key] || []
      let oldValue: string

      if (targetVersionId) {
        // 回退到用户指定的任意版本（RV-04：任意版本回退）
        const targetVer = versions.find(v => v.versionId === targetVersionId)
        oldValue = targetVer?.value ?? ''
      } else {
        // 撤销本轮修改：用 versionId 精确匹配（修复 findIndex 按值匹配的重复值缺陷）
        const thisVersionId = record.versionIds[t.key]
        if (thisVersionId) {
          const verIdx = versions.findIndex(v => v.versionId === thisVersionId)
          // 回退到该版本之前的值
          oldValue = verIdx > 0 ? versions[verIdx - 1].value : versions[0]?.value ?? ''
        } else {
          // 降级：按值查找最后一个匹配
          const lastIdx = versions.map(v => v.value).lastIndexOf(t.value)
          oldValue = lastIdx > 0 ? versions[lastIdx - 1].value : ''
        }
      }

      // 回退 = 写回旧值
      try {
        await window.blessstar.executeTool('write_config_value', { key: t.key, value: oldValue })
        console.log('[Rollback]', t.key, '→', oldValue)
      } catch (e) {
        console.warn('[Rollback] 回退写入失败:', t.key, e)
      }
    }

    // 从版本注册表中移除被回退的版本（该次管线创建的版本）
    for (const t of targets) {
      const thisVersionId = record.versionIds[t.key]
      if (thisVersionId) {
        const versions = registry[t.key] || []
        const idx = versions.findIndex(v => v.versionId === thisVersionId)
        if (idx >= 0) versions.splice(idx, 1)
        registry[t.key] = versions
      }
    }
    await saveVersionRegistry(registry)

    // 删除该用户消息及其跟随的 assistant/tool 回复
    setMessages(prev => {
      const newMsgs = prev.slice(0, messageIndex)
      let skipEnd = messageIndex + 1
      while (skipEnd < prev.length && prev[skipEnd].role !== 'user') {
        skipEnd++
      }
      return [...newMsgs, ...prev.slice(skipEnd)]
    })

    // 清理回退消息关联的 writes
    messageWritesRef.current.delete(messageIndex)

    // 用户原文填入输入框
    setInputText(rollbackText)
    setRollbackMenu({ messageIndex: -1, open: false })
  }, [messages])

  if (!isOpen) return null

  // ── 面板尺寸样式 ──
  const panelStyle = effectivePosition === 'right'
    ? { width: `${panelSize}px`, height: '100%' }
    : { height: `${panelSize}px`, width: '100%' }

  const panelBorder = effectivePosition === 'right'
    ? 'border-l border-surface-200 dark:border-surface-700'
    : 'border-t border-surface-200 dark:border-surface-700'

  // ── Resize handle position: left edge for right panel, top edge for bottom panel ──
  const resizeHandleClass = effectivePosition === 'right'
    ? 'absolute top-0 left-0 w-1 h-full cursor-ew-resize hover:bg-blue-400/40 active:bg-blue-400/60 transition-colors'
    : 'absolute top-0 left-0 w-full h-1 cursor-ns-resize hover:bg-blue-400/40 active:bg-blue-400/60 transition-colors'

  // ── Liquid Glass 水滴透明玻璃：极轻底色 + 微模糊 + 仅边缘高光（无内部反光） ──
  // 核心：不使用 inset box-shadow（会向内部渐变），改用多色 border
  // 深色模式→浅色玻璃（白色水滴透镜）；浅色模式→深色玻璃
  const lg = {
    bg: isDark ? 'rgba(255, 255, 255, 0.04)' : 'rgba(0, 0, 0, 0.03)',
    blur: 'blur(8px) saturate(150%)',
    borderTop: isDark ? 'rgba(255,255,255,0.35)' : 'rgba(255,255,255,0.22)',
    borderLeft: isDark ? 'rgba(255,255,255,0.32)' : 'rgba(255,255,255,0.20)',
    borderRight: isDark ? 'rgba(255,255,255,0.12)' : 'rgba(255,255,255,0.06)',
    borderBottom: isDark ? 'rgba(255,255,255,0.10)' : 'rgba(255,255,255,0.04)',
    // 面板作为"地面"——极弱的单层环境阴影，不出挑
    shadow: isDark
      ? '0 2px 8px rgba(0,0,0,0.12)'
      : '0 2px 8px rgba(0,0,0,0.06)',
  }

  return (
    <aside className={`relative flex flex-col overflow-hidden ${panelBorder}`}
      style={{
        ...panelStyle,
        background: lg.bg,
        backdropFilter: lg.blur,
        WebkitBackdropFilter: lg.blur,
        borderTop: `1px solid ${lg.borderTop}`,
        borderLeft: `1px solid ${lg.borderLeft}`,
        borderRight: `1px solid ${lg.borderRight}`,
        borderBottom: `1px solid ${lg.borderBottom}`,
        boxShadow: lg.shadow,
      }}
    >
      {/* 内容包装层 — 内部纯净，无反光层 */}
      <div className="relative flex flex-col h-full z-[1]">
      {/* Resize handle */}
      <div
        className={resizeHandleClass}
        onMouseDown={(e) => { e.preventDefault(); setIsResizing(true) }}
      />

      {/* Header */}
      <div className="flex items-center justify-between px-4 shrink-0"
        style={{
          minHeight: '3rem',
          borderBottom: `1px solid ${isDark ? 'rgba(255,255,255,0.10)' : 'rgba(0,0,0,0.08)'}`,
        }}
      >
        <div className="flex items-center gap-2 shrink-0">
          <span className="text-sm font-medium" style={{ color: isDark ? '#e2e8f0' : '#1e293b' }}>AI 助手</span>
          <span className="text-[10px] px-1.5 py-0.5 rounded"
            style={{
              color: isDark ? '#94a3b8' : '#475569',
              background: isDark ? 'rgba(255, 255, 255, 0.05)' : 'rgba(0, 0, 0, 0.04)',
              border: `1px solid ${isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)'}`,
            }}
          >
            {provider === 'ollama' ? '本地' : provider === 'deepseek' ? 'DeepSeek' : 'GPT'}
          </span>
          <button
            onClick={() => setShowSettings(!showSettings)}
            className="p-1 rounded transition-colors"
            style={{
              color: isDark ? '#94a3b8' : '#475569',
              background: 'transparent',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)' }}
            onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
            title="模型设置"
          >
            <svg className="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.066 2.573c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.573 1.066c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.066-2.573c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z" />
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
            </svg>
          </button>
        </div>
        <div className="flex items-center gap-1">
          <button
            onClick={() => setEffectivePosition(effectivePosition === 'right' ? 'bottom' : 'right')}
            className="p-1.5 rounded transition-colors"
            style={{ color: isDark ? '#94a3b8' : '#475569' }}
            onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)' }}
            onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
            title={effectivePosition === 'right' ? '切换到底部' : '切换到右侧'}
          >
            {effectivePosition === 'right' ? (
              <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5l7 7-7 7" />
              </svg>
            ) : (
              <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
              </svg>
            )}
          </button>
          <button
            onClick={handleClear}
            className="p-1.5 rounded transition-colors"
            style={{ color: isDark ? '#94a3b8' : '#475569' }}
            onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)' }}
            onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
            title="清除对话"
          >
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
            </svg>
          </button>
          <button
            onClick={onClose}
            className="p-1.5 rounded transition-colors"
            style={{ color: isDark ? '#94a3b8' : '#475569' }}
            onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)' }}
            onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
            title="关闭 AI 面板"
          >
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>
      </div>

      {/* 模型设置面板 */}
      {showSettings && (
        <div className="px-4 py-3 space-y-2 text-sm shrink-0"
          style={{
            borderBottom: `1px solid ${isDark ? 'rgba(255,255,255,0.10)' : 'rgba(0,0,0,0.08)'}`,
            background: isDark ? 'rgba(255, 255, 255, 0.05)' : 'rgba(0, 0, 0, 0.04)',
          }}
        >
          <div>
            <label className="text-xs mb-1 block" style={{ color: isDark ? '#94a3b8' : '#64748b' }}>模型提供商</label>
            <div className="flex gap-1">
              {(['ollama', 'deepseek', 'openai'] as const).map((p) => (
                <button
                  key={p}
                  onClick={() => { setProvider(p); reconnectBridge(p, apiKey, ollamaModel) }}
                  className="flex-1 px-2 py-1.5 rounded text-xs font-medium transition-colors"
                  style={
                    provider === p
                      ? { background: 'rgba(59, 130, 246, 0.90)', color: 'white' }
                      : {
                          background: isDark ? 'rgba(255, 255, 255, 0.08)' : 'rgba(0, 0, 0, 0.05)',
                          color: isDark ? '#cbd5e1' : '#475569',
                          border: `1px solid ${isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)'}`,
                        }
                  }
                  onMouseEnter={(e) => {
                    if (provider !== p) {
                      e.currentTarget.style.background = isDark ? 'rgba(51, 65, 85, 0.60)' : 'rgba(226, 232, 240, 0.80)'
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (provider !== p) {
                      e.currentTarget.style.background = isDark ? 'rgba(30, 41, 59, 0.55)' : 'rgba(241, 245, 249, 0.70)'
                    }
                  }}
                >
                  {p === 'ollama' ? 'Ollama' : p === 'deepseek' ? 'DeepSeek' : 'OpenAI'}
                </button>
              ))}
            </div>
          </div>
          {provider !== 'ollama' && (
            <div>
              <label className="text-xs mb-1 block" style={{ color: isDark ? '#94a3b8' : '#64748b' }}>API Key</label>
              <input
                type="password"
                value={apiKey}
                onChange={(e) => { setApiKey(e.target.value); reconnectBridge(provider, e.target.value, ollamaModel) }}
                placeholder={provider === 'deepseek' ? 'sk-...' : 'sk-proj-...'}
                className="w-full px-2 py-1.5 text-xs rounded focus:outline-none"
                style={{
                  border: `1px solid ${isDark ? 'rgba(255,255,255,0.20)' : 'rgba(0,0,0,0.15)'}`,
                  background: isDark ? 'rgba(255, 255, 255, 0.08)' : 'rgba(0, 0, 0, 0.05)',
                  color: isDark ? '#e2e8f0' : '#1e293b',
                }}
              />
            </div>
          )}
          {provider === 'ollama' && (
            <div>
              <label className="text-xs mb-1 block" style={{ color: isDark ? '#94a3b8' : '#64748b' }}>模型名称</label>
              <input
                type="text"
                value={ollamaModel}
                onChange={(e) => { setOllamaModel(e.target.value); reconnectBridge(provider, apiKey, e.target.value) }}
                placeholder="qwen2.5-coder:7b"
                className="w-full px-2 py-1.5 text-xs rounded focus:outline-none"
                style={{
                  border: `1px solid ${isDark ? 'rgba(255,255,255,0.20)' : 'rgba(0,0,0,0.15)'}`,
                  background: isDark ? 'rgba(255, 255, 255, 0.08)' : 'rgba(0, 0, 0, 0.05)',
                  color: isDark ? '#e2e8f0' : '#1e293b',
                }}
              />
            </div>
          )}
        </div>
      )}

      {/* Messages — 保持透明，让 AIPanel 外壳背后的编辑器内容能透出来被毛玻璃模糊 */}
      <div className="flex-1 overflow-y-auto p-3 space-y-3">
        {messages.map((msg, i) => {
          const isLatest = i === messages.length - 1
          const hasSandbox = msg.planSteps && msg.planSteps.length > 0
          const hasWrites = msg.role === 'user' && messageWritesRef.current.has(i)
          const msgWrites = hasWrites ? messageWritesRef.current.get(i)!.writes : []
          const isRollbackOpen = rollbackMenu.messageIndex === i && rollbackMenu.open

          return (
          <div key={i} className={msg.role === 'user' ? 'flex justify-end' : 'space-y-2'}>
            {/* ── 用户消息 + 回退按钮（第33天 · RV-06）─────────────── */}
            {msg.role === 'user' && (
              <div className="flex items-start gap-1">
                {hasWrites && (
                  <div className="relative">
                    <button
                      ref={(el) => { rollbackBtnRef.current.set(i, el) }}
                      onClick={(e) => {
                        e.stopPropagation()
                        setRollbackMenu(prev =>
                          prev.messageIndex === i
                            ? { messageIndex: i, open: !prev.open }
                            : { messageIndex: i, open: true }
                        )
                      }}
                      className="shrink-0 w-6 h-6 rounded-full flex items-center justify-center text-xs transition-all mt-1"
                      style={{
                        color: isDark ? '#94a3b8' : '#64748b',
                        background: isRollbackOpen
                          ? (isDark ? 'rgba(255,255,255,0.12)' : 'rgba(0,0,0,0.08)')
                          : 'transparent',
                      }}
                      onMouseEnter={(e) => {
                        if (!isRollbackOpen) {
                          e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)'
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (!isRollbackOpen) {
                          e.currentTarget.style.background = 'transparent'
                        }
                      }}
                      title="回退此对话的配置修改"
                    >
                      ↩
                    </button>
                    {isRollbackOpen && (
                      <div
                        className="absolute left-0 top-8 z-20 min-w-[220px] max-w-[320px] rounded-lg p-2 shadow-lg"
                        style={{
                          background: isDark ? 'rgba(30, 41, 59, 0.95)' : 'rgba(255, 255, 255, 0.95)',
                          border: `1px solid ${isDark ? 'rgba(255,255,255,0.15)' : 'rgba(0,0,0,0.12)'}`,
                          backdropFilter: 'blur(12px)',
                        }}
                      >
                        <button
                          onClick={(e) => { e.stopPropagation(); handleRollback(i) }}
                          className="w-full text-left px-3 py-1.5 rounded text-sm transition-colors"
                          style={{
                            color: isDark ? '#e2e8f0' : '#1e293b',
                          }}
                          onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.1)' : 'rgba(0,0,0,0.05)' }}
                          onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
                        >
                          撤销此对话所有配置修改
                        </button>
                        {msgWrites.length > 1 && (
                          <div className="my-1 border-t"
                            style={{ borderColor: isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.06)' }}
                          />
                        )}
                        {msgWrites.map((w) => {
                          const isSubOpen = rollbackMenu.subKey === w.key
                          const keyVersions = rollbackVersions[w.key] || []
                          return (
                          <div key={w.key}>
                            <button
                              onClick={async (e) => {
                                e.stopPropagation()
                                if (!isSubOpen) {
                                  // 加载版本注册表，获取该 key 的全部版本
                                  const { loadVersionRegistry } = await import('./versionRegistry')
                                  const reg = await loadVersionRegistry()
                                  setRollbackVersions(reg)
                                }
                                setRollbackMenu(prev => ({
                                  ...prev,
                                  subKey: isSubOpen ? undefined : w.key,
                                }))
                              }}
                              className="w-full text-left px-3 py-1.5 rounded text-sm transition-colors flex items-center justify-between"
                              style={{ color: isDark ? '#cbd5e1' : '#475569' }}
                              onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.1)' : 'rgba(0,0,0,0.05)' }}
                              onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
                            >
                              <span>{w.key.split('.').pop() || w.key} 的版本</span>
                              <svg className={`w-3 h-3 transition-transform ${isSubOpen ? 'rotate-90' : ''}`} fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5l7 7-7 7" />
                              </svg>
                            </button>
                            {isSubOpen && keyVersions.length > 0 && (
                              <div className="ml-2 mt-0.5 mb-1 border-l-2 pl-2"
                                style={{ borderColor: isDark ? 'rgba(255,255,255,0.10)' : 'rgba(0,0,0,0.08)' }}
                              >
                                <button
                                  onClick={(e) => { e.stopPropagation(); handleRollback(i, w.key) }}
                                  className="w-full text-left px-2 py-1 rounded text-xs transition-colors"
                                  style={{ color: isDark ? '#93c5fd' : '#2563eb' }}
                                  onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.04)' }}
                                  onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
                                >
                                  撤销本次修改
                                </button>
                                {keyVersions.map((v) => (
                                  <button
                                    key={v.versionId}
                                    onClick={(e) => { e.stopPropagation(); handleRollback(i, w.key, v.versionId) }}
                                    className="w-full text-left px-2 py-1 rounded text-xs transition-colors flex items-center justify-between"
                                    style={{ color: isDark ? '#94a3b8' : '#64748b' }}
                                    onMouseEnter={(e) => { e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.04)' }}
                                    onMouseLeave={(e) => { e.currentTarget.style.background = 'transparent' }}
                                  >
                                    <span className="font-mono truncate mr-2">{v.displayName || v.versionId}</span>
                                    <span className="text-surface-400 shrink-0">{v.value}</span>
                                  </button>
                                ))}
                              </div>
                            )}
                          </div>
                        )})}
                      </div>
                    )}
                  </div>
                )}
                {/* Text bubble */}
                {msg.content && (
                  <div
                    className="max-w-[85%] rounded-lg px-3 py-2 text-sm whitespace-pre-wrap"
                    style={{
                      background: isDark ? 'rgba(59, 130, 246, 0.70)' : 'rgba(59, 130, 246, 0.75)',
                      color: 'white',
                      borderTop: `1px solid rgba(255,255,255,0.35)`,
                      borderLeft: `1px solid rgba(255,255,255,0.30)`,
                      borderRight: `1px solid rgba(59,130,246,0.40)`,
                      borderBottom: `1px solid rgba(59,130,246,0.35)`,
                      boxShadow: isDark
                        ? '0 2px 12px rgba(59, 130, 246, 0.25)'
                        : '0 2px 12px rgba(59, 130, 246, 0.20)',
                    }}
                  >
                    {msg.content}
                  </div>
                )}
              </div>
            )}
            {/* ── 非用户消息保持不变 ─────────────────────────────── */}
            {msg.role !== 'user' && msg.content && (
              <div className="flex justify-start">
                <div
                  className="max-w-[85%] rounded-lg px-3 py-2 text-sm whitespace-pre-wrap"
                  style={
                    msg.role === 'tool'
                      ? {
                          background: isDark ? 'rgba(255, 255, 255, 0.05)' : 'rgba(0, 0, 0, 0.03)',
                          color: isDark ? '#94a3b8' : '#475569',
                          fontSize: '12px',
                          fontFamily: 'monospace',
                          border: `1px solid ${isDark ? 'rgba(255,255,255,0.15)' : 'rgba(0,0,0,0.10)'}`,
                        }
                      : {
                          background: isDark ? 'rgba(255, 255, 255, 0.05)' : 'rgba(0, 0, 0, 0.03)',
                          color: isDark ? '#e2e8f0' : '#1e293b',
                          border: `1px solid ${isDark ? 'rgba(255,255,255,0.15)' : 'rgba(0,0,0,0.10)'}`,
                        }
                  }
                >
                  {msg.content}
                </div>
              </div>
            )}
            {/* Sandbox — independent block, full width */}
            {hasSandbox && (
              <div className="w-full">
                <SandboxTodo
                  steps={msg.planSteps!}
                  thinking={msg.thinking}
                  toolCards={msg.toolCards}
                  isLatest={isLatest}
                />
              </div>
            )}
          </div>
        )})}
        {isProcessing && (
          <div className="flex justify-start">
            <div className="rounded-lg px-3 py-2"
              style={{
                background: isDark ? 'rgba(255, 255, 255, 0.05)' : 'rgba(0, 0, 0, 0.03)',
                border: `1px solid ${isDark ? 'rgba(255,255,255,0.15)' : 'rgba(0,0,0,0.10)'}`,
              }}
            >
              <div className="flex gap-1">
                <div className="w-2 h-2 rounded-full animate-bounce" style={{ background: isDark ? '#94a3b8' : '#64748b', animationDelay: '0ms' }} />
                <div className="w-2 h-2 rounded-full animate-bounce" style={{ background: isDark ? '#94a3b8' : '#64748b', animationDelay: '150ms' }} />
                <div className="w-2 h-2 rounded-full animate-bounce" style={{ background: isDark ? '#94a3b8' : '#64748b', animationDelay: '300ms' }} />
              </div>
            </div>
          </div>
        )}
        {/* ASK 选择器 — 当管线挂起等待用户选择时显示 */}
        {awaitingConfirmationRef.current && suspendedStateRef.current && (
          <div className="w-full">
            <AskSelector
              question={suspendedStateRef.current.question}
              candidates={suspendedStateRef.current.candidates}
              onSelect={handleAskSelect}
              loading={askLoading}
            />
          </div>
        )}
        <div ref={messagesEndRef} />
      </div>

      {/* Suggestion accept bar — only show when currentSuggestion has meaningful content */}
      {currentSuggestion && (
        <div className="px-3 py-2 shrink-0"
          style={{
            borderTop: `1px solid ${isDark ? 'rgba(147, 197, 253, 0.30)' : 'rgba(59, 130, 246, 0.30)'}`,
            background: isDark ? 'rgba(59, 130, 246, 0.15)' : 'rgba(59, 130, 246, 0.10)',
          }}
        >
          <div className="flex items-center justify-between mb-1">
            <span className="text-xs font-medium" style={{ color: isDark ? '#93c5fd' : '#1d4ed8' }}>📋 有可采纳的编辑建议</span>
            <button
              onClick={handleAcceptSuggestion}
              className="shrink-0 px-3 py-1 text-white text-xs rounded transition-colors ml-3"
              style={{ background: 'rgba(59, 130, 246, 0.90)' }}
              onMouseEnter={(e) => { e.currentTarget.style.background = 'rgba(37, 99, 235, 0.95)' }}
              onMouseLeave={(e) => { e.currentTarget.style.background = 'rgba(59, 130, 246, 0.90)' }}
            >
              采纳建议
            </button>
          </div>
          <div
            className="text-xs whitespace-pre-wrap break-all line-clamp-3 rounded px-2 py-1.5"
            style={{
              color: isDark ? '#cbd5e1' : '#334155',
              background: isDark ? 'rgba(0,0,0,0.20)' : 'rgba(255,255,255,0.40)',
              border: `1px solid ${isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.06)'}`,
              fontFamily: 'monospace',
            }}
          >
            {currentSuggestion.length > 200 ? currentSuggestion.slice(0, 200) + '...' : currentSuggestion}
          </div>
        </div>
      )}

      {/* Input area */}
      <div className="px-3 py-2 shrink-0"
        style={{
          borderTop: `1px solid ${isDark ? 'rgba(255,255,255,0.18)' : 'rgba(0,0,0,0.12)'}`,
          background: isDark ? 'rgba(255, 255, 255, 0.04)' : 'rgba(0, 0, 0, 0.03)',
        }}
      >
        {/* 命令提示下拉列表 */}
        {showCmdSuggestions && cmdSuggestions.length > 0 && (
          <div
            className="mb-1 rounded-lg overflow-hidden shadow-lg"
            style={{
              border: `1px solid ${isDark ? 'rgba(255,255,255,0.15)' : 'rgba(0,0,0,0.10)'}`,
              background: isDark ? 'rgba(30, 41, 59, 0.95)' : 'rgba(255, 255, 255, 0.95)',
              backdropFilter: 'blur(12px)',
              maxHeight: '200px',
              overflowY: 'auto',
            }}
          >
            {cmdSuggestions.map((cmd, idx) => (
              <div
                key={cmd.command}
                onClick={() => {
                  setInputText(cmd.command + ' ')
                  setShowCmdSuggestions(false)
                  inputRef.current?.focus()
                }}
                className="px-3 py-2 cursor-pointer flex items-center justify-between transition-colors"
                style={{
                  background: idx === selectedCmdIndex
                    ? (isDark ? 'rgba(59, 130, 246, 0.25)' : 'rgba(59, 130, 246, 0.15)')
                    : 'transparent',
                }}
                onMouseEnter={(e) => {
                  if (idx !== selectedCmdIndex) {
                    e.currentTarget.style.background = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)'
                  }
                }}
                onMouseLeave={(e) => {
                  if (idx !== selectedCmdIndex) {
                    e.currentTarget.style.background = 'transparent'
                  }
                }}
              >
                <div className="flex items-center gap-2">
                  <span className="text-sm font-mono font-medium" style={{ color: isDark ? '#93c5fd' : '#2563eb' }}>
                    {cmd.command}
                  </span>
                  <span className="text-xs px-1.5 py-0.5 rounded" style={{
                    color: isDark ? '#94a3b8' : '#64748b',
                    background: isDark ? 'rgba(255,255,255,0.08)' : 'rgba(0,0,0,0.05)',
                  }}>
                    {cmd.intent}
                  </span>
                </div>
                <span className="text-xs" style={{ color: isDark ? '#94a3b8' : '#64748b' }}>
                  {cmd.description}
                </span>
              </div>
            ))}
          </div>
        )}
        <textarea
          ref={inputRef}
          value={inputText}
          onChange={(e) => {
            const val = e.target.value
            setInputText(val)
            filterCommands(val)
          }}
          onKeyDown={handleKeyDown}
          placeholder="输入消息... (Enter 发送, Shift+Enter 换行)"
          rows={2}
          className="w-full px-3 py-2 text-sm rounded resize-none focus:outline-none"
          style={{
            border: `1px solid ${isDark ? 'rgba(255,255,255,0.20)' : 'rgba(0,0,0,0.15)'}`,
            background: isDark ? 'rgba(255, 255, 255, 0.08)' : 'rgba(0, 0, 0, 0.05)',
            color: isDark ? '#e2e8f0' : '#1e293b',
          }}
        />
      </div>
      </div>{/* close content wrapper */}
    </aside>
  )
}
