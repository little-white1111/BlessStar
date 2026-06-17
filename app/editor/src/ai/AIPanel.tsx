import { useState, useRef, useEffect, useCallback } from 'react'
import type { AIMessage, AICompletionRequest } from './types'
import { getToolDefinitions } from './tools'
import { createAIBridge } from './bridge'
import { executeToolCall } from './executor'
import { buildContext } from './context-manager/contextBuilder'
import { buildToolDelta } from './context-manager/toolDeltaFormatter'
import type { CompactIndex, ToolDelta } from './context-manager/contextBuilder'

type PanelPosition = 'right' | 'bottom'

interface AIPanelProps {
  isOpen: boolean
  onClose: () => void
  initialPosition?: PanelPosition
  onAcceptSuggestion?: (suggestion: string) => void
}

const SYSTEM_PROMPT = `你是 BlessStar 配置编辑器的 AI 助手。你可以使用以下工具帮助用户：

1. create_schema_field - 创建 Schema 字段定义
2. update_gate_rule - 更新 Gate 门禁规则
3. validate_config - 校验配置 JSON
4. suggest_field_type - 推荐控件类型
5. generate_normalizer_template - 生成厂商归一化模板

每次操作前请先与用户确认意图，然后调用对应工具。工具执行结果将通过 BlessStar 校验器验证。`

export function AIPanel({ isOpen, onClose, initialPosition = 'right', onAcceptSuggestion }: AIPanelProps) {
  const [position, setPosition] = useState<PanelPosition>(initialPosition)
  // messages 仅用于 UI 渲染，不参与模型请求
  const [messages, setMessages] = useState<AIMessage[]>([
    { role: 'assistant', content: '您好！我是 AI 配置助手，可以帮您创建 Schema 字段、配置 Gate 规则、校验配置等。请问需要什么帮助？' },
  ])
  const [inputText, setInputText] = useState('')
  const [isProcessing, setIsProcessing] = useState(false)
  const [currentSuggestion, setCurrentSuggestion] = useState<string | null>(null)
  const messagesEndRef = useRef<HTMLDivElement>(null)
  const inputRef = useRef<HTMLTextAreaElement>(null)
  const bridgeRef = useRef(createAIBridge({
    provider: 'ollama',
    ollamaUrl: 'http://localhost:11434',
    ollamaModel: 'qwen2.5-coder:7b',
  }))
  // 上一轮 tool delta，用于 contextBuilder 注入工作记忆
  const lastToolDeltaRef = useRef<ToolDelta | undefined>(undefined)

  const scrollToBottom = useCallback(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [])

  useEffect(() => {
    scrollToBottom()
  }, [messages, scrollToBottom])

  useEffect(() => {
    if (isOpen) {
      inputRef.current?.focus()
    }
  }, [isOpen])

  const handleSend = useCallback(async () => {
    const text = inputText.trim()
    if (!text || isProcessing) return

    setInputText('')
    setCurrentSuggestion(null)

    // UI 显示用户消息
    const userMsg: AIMessage = { role: 'user', content: text }
    setMessages((prev) => [...prev, userMsg])
    setIsProcessing(true)

    try {
      // 使用 contextBuilder 构建固定长度的上下文
      const contextMessages = buildContext({
        userInput: text,
        systemPrompt: SYSTEM_PROMPT,
        toolDefs: getToolDefinitions(),
        indexCompact: null, // 暂无预生成 compact 索引文件；后续从文件读取
        lastToolDelta: lastToolDeltaRef.current,
      })

      // 发送给模型（不走历史累积）
      const req: AICompletionRequest = {
        messages: contextMessages,
        tools: getToolDefinitions(),
      }

      const response = await bridgeRef.current.complete(req)
      const assistantMsg: AIMessage = { role: 'assistant', content: response.message.content }
      setMessages((prev) => [...prev, assistantMsg])

      // Handle tool calls
      if (response.tool_calls && response.tool_calls.length > 0) {
        const toolCall = response.tool_calls[0]
        const result = await executeToolCall(toolCall)

        // Check if result has suggestion content to accept
        if (result.success && result.data) {
          const dataStr = typeof result.data === 'string'
            ? result.data
            : JSON.stringify(result.data, null, 2)
          setCurrentSuggestion(dataStr)
        }

        // 构建单行摘要 tool delta（供下一轮使用）
        lastToolDeltaRef.current = buildToolDelta(toolCall.function.name, {
          success: result.success,
          data: result.data,
          error: result.error,
        })

        // UI 显示 tool 结果
        const toolResultMsg: AIMessage = {
          role: 'tool',
          content: result.success
            ? JSON.stringify(result.data, null, 2)
            : `错误: ${result.error}`,
          tool_call_id: toolCall.id,
        }
        setMessages((prev) => [...prev, toolResultMsg])

        // 请求 follow-up 摘要
        const followUpContext = buildContext({
          userInput: text,
          systemPrompt: SYSTEM_PROMPT,
          toolDefs: getToolDefinitions(),
          indexCompact: null,
          lastToolDelta: lastToolDeltaRef.current,
        })

        const followUpReq: AICompletionRequest = {
          messages: followUpContext,
        }
        const followUp = await bridgeRef.current.complete(followUpReq)
        setMessages((prev) => [...prev, followUp.message])
      }
    } catch (err) {
      const errMsg = err instanceof Error ? err.message : '未知错误'
      setMessages((prev) => [...prev, { role: 'assistant', content: `处理请求时出错：${errMsg}` }])
    } finally {
      setIsProcessing(false)
    }
  }, [inputText, isProcessing])

  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      handleSend()
    }
  }, [handleSend])

  const handleAcceptSuggestion = useCallback(() => {
    if (currentSuggestion && onAcceptSuggestion) {
      onAcceptSuggestion(currentSuggestion)
      setCurrentSuggestion(null)
      setMessages((prev) => [...prev, {
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
  }, [])

  if (!isOpen) return null

  const panelClass = position === 'right'
    ? 'w-80 border-l border-surface-200 dark:border-surface-700'
    : 'h-64 border-t border-surface-200 dark:border-surface-700'

  return (
    <aside className={`flex flex-col bg-white dark:bg-surface-800 overflow-hidden ${panelClass}`}>
      {/* Header */}
      <div className="h-12 flex items-center justify-between px-4 border-b border-surface-200 dark:border-surface-700 shrink-0">
        <div className="flex items-center gap-2">
          <span className="text-sm font-medium text-surface-700 dark:text-surface-300">AI 助手</span>
          <span className="text-[10px] text-surface-400 bg-surface-100 dark:bg-surface-700 px-1.5 py-0.5 rounded">
            Beta
          </span>
        </div>
        <div className="flex items-center gap-1">
          {/* Position toggle */}
          <button
            onClick={() => setPosition(position === 'right' ? 'bottom' : 'right')}
            className="p-1.5 rounded hover:bg-surface-100 dark:hover:bg-surface-700 text-surface-400 hover:text-surface-600 dark:hover:text-surface-300"
            title={position === 'right' ? '切换到底部' : '切换到右侧'}
          >
            {position === 'right' ? (
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
            className="p-1.5 rounded hover:bg-surface-100 dark:hover:bg-surface-700 text-surface-400 hover:text-surface-600 dark:hover:text-surface-300"
            title="清除对话"
          >
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
            </svg>
          </button>
          <button
            onClick={onClose}
            className="p-1.5 rounded hover:bg-surface-100 dark:hover:bg-surface-700 text-surface-400 hover:text-surface-600 dark:hover:text-surface-300"
            title="关闭 AI 面板"
          >
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>
        </div>
      </div>

      {/* Messages - 仅用于 UI 渲染 */}
      <div className="flex-1 overflow-y-auto p-3 space-y-3">
        {messages.map((msg, i) => (
          <div key={i} className={`flex ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
            <div
              className={`max-w-[85%] rounded-lg px-3 py-2 text-sm whitespace-pre-wrap ${
                msg.role === 'user'
                  ? 'bg-primary-500 text-white'
                  : msg.role === 'tool'
                  ? 'bg-surface-100 dark:bg-surface-700 text-surface-600 dark:text-surface-400 text-xs font-mono'
                  : 'bg-surface-100 dark:bg-surface-700 text-surface-800 dark:text-surface-200'
              }`}
            >
              {msg.content}
            </div>
          </div>
        ))}
        {isProcessing && (
          <div className="flex justify-start">
            <div className="bg-surface-100 dark:bg-surface-700 rounded-lg px-3 py-2">
              <div className="flex gap-1">
                <div className="w-2 h-2 bg-surface-400 rounded-full animate-bounce" style={{ animationDelay: '0ms' }} />
                <div className="w-2 h-2 bg-surface-400 rounded-full animate-bounce" style={{ animationDelay: '150ms' }} />
                <div className="w-2 h-2 bg-surface-400 rounded-full animate-bounce" style={{ animationDelay: '300ms' }} />
              </div>
            </div>
          </div>
        )}
        <div ref={messagesEndRef} />
      </div>

      {/* Suggestion accept bar */}
      {currentSuggestion && (
        <div className="px-3 py-2 border-t border-surface-200 dark:border-surface-700 bg-primary-50 dark:bg-primary-900/20">
          <div className="flex items-center justify-between">
            <span className="text-xs text-primary-700 dark:text-primary-300">有新建议可采纳</span>
            <button
              onClick={handleAcceptSuggestion}
              className="px-3 py-1 bg-primary-500 text-white text-xs rounded hover:bg-primary-600 transition-colors"
            >
              采纳建议
            </button>
          </div>
        </div>
      )}

      {/* Input area */}
      <div className="p-3 border-t border-surface-200 dark:border-surface-700 shrink-0">
        <div className="flex gap-2">
          <textarea
            ref={inputRef}
            value={inputText}
            onChange={(e) => setInputText(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder="输入提示，按 Enter 发送（Shift+Enter 换行）"
            rows={2}
            className="flex-1 resize-none text-sm px-3 py-2 rounded border border-surface-300 dark:border-surface-600 bg-white dark:bg-surface-700 text-surface-900 dark:text-surface-100 placeholder-surface-400 focus:outline-none focus:ring-2 focus:ring-primary-400 focus:border-transparent"
            disabled={isProcessing}
          />
          <button
            onClick={handleSend}
            disabled={!inputText.trim() || isProcessing}
            className="self-end px-3 py-2 bg-primary-500 text-white rounded hover:bg-primary-600 disabled:opacity-40 disabled:cursor-not-allowed transition-colors"
          >
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 19V5m0 0l-7 7m7-7l7 7" />
            </svg>
          </button>
        </div>
        <p className="mt-1 text-[10px] text-surface-400 dark:text-surface-500">
          支持 5 种工具：Schema 字段 / Gate 规则 / 配置校验 / 字段类型推荐 / 归一化模板
        </p>
      </div>
    </aside>
  )
}

export default AIPanel
