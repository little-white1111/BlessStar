/**
 * AskSelector — ASK 配置项选择器 UI 组件
 *
 * 当用户输入的配置项名称未精确匹配时，显示带编号的候选列表供选择。
 * 底部固定一个"其他"选项，用户可输入自定义描述。
 * 选择后自动注册别名映射，或走回滚 loop 重新执行管线。
 */

import { useState } from 'react'

export interface AskCandidate {
  label: string
  configKey: string
  aiHint: string
  /** D38-4-OPT-A: 此项渲染为文本输入框（路径为空时由用户输入值） */
  textInput?: boolean
}

interface AskSelectorProps {
  question: string
  candidates: AskCandidate[]
  onSelect: (candidate: AskCandidate | null) => void
  /** 等待管线重新执行中 */
  loading?: boolean
}

const LETTERS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'

export function AskSelector({ question, candidates, onSelect, loading }: AskSelectorProps) {
  const [selectedIdx, setSelectedIdx] = useState<number | null>(null)
  const [textInputValue, setTextInputValue] = useState('')

  // "其他" 永远是最后一个选项
  const otherIdx = candidates.length
  const isOtherSelected = selectedIdx === otherIdx

  const handleConfirm = () => {
    if (selectedIdx === null) {
      // 没选任何东西 → 取消
      onSelect(null)
      return
    }
    if (isOtherSelected) {
      // 其他 + 输入文本 → 自定义查询回滚 loop
      if (textInputValue.trim()) {
        onSelect({ label: '其他', configKey: `__custom_query__:${textInputValue.trim()}`, aiHint: '' })
      }
      return
    }
    // 正常候选
    const c = candidates[selectedIdx]
    if (c.textInput && textInputValue.trim()) {
      onSelect({ ...c, configKey: `__input__:${textInputValue.trim()}` })
    } else if (!c.textInput) {
      onSelect(c)
    }
  }

  if (loading) {
    return (
      <div className="bg-white/80 dark:bg-gray-800/80 backdrop-blur-sm rounded-lg border border-gray-200 dark:border-gray-700 p-4 space-y-3">
        <p className="text-sm text-gray-600 dark:text-gray-300">{question}</p>
        <div className="flex items-center justify-center py-3">
          <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-blue-500" />
          <span className="ml-2 text-sm text-gray-500">处理中...</span>
        </div>
      </div>
    )
  }

  return (
    <div className="bg-white/80 dark:bg-gray-800/80 backdrop-blur-sm rounded-lg border border-gray-200 dark:border-gray-700 p-4 space-y-3">
      <p className="text-sm font-medium text-gray-700 dark:text-gray-200">{question}</p>

      <div className="space-y-2">
        {/* 候选选项 */}
        {candidates.slice(0, 8).map((c, i) =>
          c.textInput ? (
            <div
              key={c.configKey}
              onClick={() => setSelectedIdx(i)}
              className={`w-full p-3 rounded-lg border transition-colors cursor-pointer ${
                selectedIdx === i
                  ? 'border-blue-500 bg-blue-50 dark:bg-blue-900/30 dark:border-blue-400'
                  : 'border-gray-200 dark:border-gray-600 hover:bg-gray-50 dark:hover:bg-gray-700/50'
              }`}
            >
              <div className="flex items-start gap-3">
                <span className={`flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center text-xs font-bold mt-0.5 ${
                  selectedIdx === i
                    ? 'bg-blue-500 text-white'
                    : 'bg-gray-200 dark:bg-gray-600 text-gray-600 dark:text-gray-300'
                }`}>
                  {LETTERS[i]}
                </span>
                <div className="min-w-0 flex-1 space-y-2">
                  <div className="text-sm font-medium text-gray-800 dark:text-gray-100">{c.label}</div>
                  {selectedIdx === i && (
                    <input
                      type="text"
                      value={textInputValue}
                      onChange={e => setTextInputValue(e.target.value)}
                      placeholder="请输入路径..."
                      className="w-full px-3 py-2 text-sm rounded-md border border-gray-300 dark:border-gray-500 bg-white dark:bg-gray-700 text-gray-900 dark:text-gray-100 focus:outline-none focus:ring-2 focus:ring-blue-400"
                      autoFocus
                      onClick={e => e.stopPropagation()}
                    />
                  )}
                  {c.aiHint && (
                    <div className="text-xs text-gray-500 dark:text-gray-400">{c.aiHint}</div>
                  )}
                </div>
              </div>
            </div>
          ) : (
          <button
            key={c.configKey}
            onClick={() => setSelectedIdx(i)}
            className={`w-full text-left p-3 rounded-lg border transition-colors ${
              selectedIdx === i
                ? 'border-blue-500 bg-blue-50 dark:bg-blue-900/30 dark:border-blue-400'
                : 'border-gray-200 dark:border-gray-600 hover:bg-gray-50 dark:hover:bg-gray-700/50'
            }`}
          >
            <div className="flex items-start gap-3">
              <span className={`flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center text-xs font-bold ${
                selectedIdx === i
                  ? 'bg-blue-500 text-white'
                  : 'bg-gray-200 dark:bg-gray-600 text-gray-600 dark:text-gray-300'
              }`}>
                {LETTERS[i]}
              </span>
              <div className="min-w-0 flex-1">
                <div className="text-sm font-medium text-gray-800 dark:text-gray-100">
                  {c.label}
                  <span className="ml-2 text-xs text-gray-400 dark:text-gray-500 font-mono">
                    {c.configKey}
                  </span>
                </div>
                {c.aiHint && (
                  <div className="text-xs text-gray-500 dark:text-gray-400 mt-0.5 line-clamp-2">
                    {c.aiHint}
                  </div>
                )}
              </div>
            </div>
          </button>
        ))}

        {/* "其他" 选项（固定在最后） */}
        <div
          onClick={() => {
            setSelectedIdx(otherIdx)
            setTextInputValue('')
          }}
          className={`w-full p-3 rounded-lg border transition-colors cursor-pointer ${
            isOtherSelected
              ? 'border-blue-500 bg-blue-50 dark:bg-blue-900/30 dark:border-blue-400'
              : 'border-gray-200 dark:border-gray-600 hover:bg-gray-50 dark:hover:bg-gray-700/50'
          }`}
        >
          <div className="flex items-start gap-3">
            <span className={`flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center text-xs font-bold mt-0.5 ${
              isOtherSelected
                ? 'bg-blue-500 text-white'
                : 'bg-gray-200 dark:bg-gray-600 text-gray-600 dark:text-gray-300'
            }`}>
              {candidates.length < 26 ? LETTERS[otherIdx] : '?'}
            </span>
            <div className="min-w-0 flex-1 space-y-2">
              <div className="text-sm font-medium text-gray-800 dark:text-gray-100">其他</div>
              {isOtherSelected && (
                <input
                  type="text"
                  value={textInputValue}
                  onChange={e => setTextInputValue(e.target.value)}
                  placeholder="输入您想要执行的操作描述..."
                  className="w-full px-3 py-2 text-sm rounded-md border border-gray-300 dark:border-gray-500 bg-white dark:bg-gray-700 text-gray-900 dark:text-gray-100 focus:outline-none focus:ring-2 focus:ring-blue-400"
                  autoFocus
                  onClick={e => e.stopPropagation()}
                />
              )}
              <div className="text-xs text-gray-500 dark:text-gray-400">
                输入新描述，系统将尝试理解并执行
              </div>
            </div>
          </div>
        </div>
      </div>

      <div className="flex gap-2 pt-1">
        <button
          onClick={handleConfirm}
          className="flex-1 py-2 px-4 rounded-lg text-sm font-medium bg-blue-500 text-white hover:bg-blue-600 transition-colors"
        >
          确认
        </button>
        <button
          onClick={() => onSelect(null)}
          className="flex-1 py-2 px-4 rounded-lg text-sm text-gray-500 dark:text-gray-400 hover:bg-gray-100 dark:hover:bg-gray-700 transition-colors border border-gray-200 dark:border-gray-600"
        >
          取消
        </button>
      </div>
    </div>
  )
}
