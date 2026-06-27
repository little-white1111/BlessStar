/**
 * ToastProvider — 全局 Toast 通知系统（第35天 · UX-01）
 *
 * 通过 React Context 注入 showToast(type, message) API。
 * 四种类型：success(绿) / warning(黄) / error(红) / loading(蓝灰)
 * auto-dismiss: success 3s / warning 5s / error 不自动消失 / loading 由调用方控制
 */

import { createContext, useCallback, useContext, useState, useEffect, type ReactNode } from 'react'

export type ToastType = 'success' | 'warning' | 'error' | 'loading'

interface Toast {
  id: number
  type: ToastType
  message: string
  createdAt: number
}

interface ToastContextValue {
  /** show a toast; returns id for programmatic dismiss (only needed for loading type) */
  showToast: (type: ToastType, message: string) => number
  /** dismiss a specific toast by id */
  dismissToast: (id: number) => void
}

const ToastContext = createContext<ToastContextValue | null>(null)

let nextId = 1

export function ToastProvider({ children }: { children: ReactNode }) {
  const [toasts, setToasts] = useState<Toast[]>([])

  const showToast = useCallback((type: ToastType, message: string): number => {
    const id = nextId++
    setToasts((prev) => [...prev, { id, type, message, createdAt: Date.now() }])
    return id
  }, [])

  const dismissToast = useCallback((id: number) => {
    setToasts((prev) => prev.filter((t) => t.id !== id))
  }, [])

  // auto-dismiss logic (excludes error and loading)
  useEffect(() => {
    const timers: ReturnType<typeof setTimeout>[] = []
    for (const toast of toasts) {
      if (toast.type === 'success') {
        timers.push(setTimeout(() => dismissToast(toast.id), 3000))
      } else if (toast.type === 'warning') {
        timers.push(setTimeout(() => dismissToast(toast.id), 5000))
      }
    }
    return () => timers.forEach(clearTimeout)
  }, [toasts, dismissToast])

  const bgColor = (t: ToastType) => {
    switch (t) {
      case 'success': return 'bg-green-50 dark:bg-green-900/20 border-green-300 dark:border-green-700 text-green-800 dark:text-green-200'
      case 'warning': return 'bg-yellow-50 dark:bg-yellow-900/20 border-yellow-300 dark:border-yellow-700 text-yellow-800 dark:text-yellow-200'
      case 'error':   return 'bg-red-50 dark:bg-red-900/20 border-red-300 dark:border-red-700 text-red-800 dark:text-red-200'
      case 'loading': return 'bg-blue-50 dark:bg-blue-900/20 border-blue-300 dark:border-blue-700 text-blue-800 dark:text-blue-200'
    }
  }

  const icon = (t: ToastType) => {
    switch (t) {
      case 'success': return '✓'
      case 'warning': return '⚠'
      case 'error':   return '✕'
      case 'loading': return '◌'
    }
  }

  return (
    <ToastContext.Provider value={{ showToast, dismissToast }}>
      {children}
      {/* Toast container — fixed bottom-right */}
      <div className="fixed bottom-4 right-4 z-[9999] flex flex-col gap-2 max-w-sm pointer-events-none">
        {toasts.map((t) => (
          <div
            key={t.id}
            className={`pointer-events-auto flex items-center gap-2 px-4 py-3 rounded-lg border shadow-md text-sm ${bgColor(t.type)}`}
            role="alert"
          >
            <span className={`font-bold ${t.type === 'loading' ? 'animate-spin inline-block' : ''}`}>
              {icon(t.type)}
            </span>
            <span className="flex-1">{t.message}</span>
            <button
              onClick={() => dismissToast(t.id)}
              className="ml-2 opacity-50 hover:opacity-100 text-current font-bold text-base leading-none"
              aria-label="关闭"
            >
              ×
            </button>
          </div>
        ))}
      </div>
    </ToastContext.Provider>
  )
}

export function useToast(): ToastContextValue {
  const ctx = useContext(ToastContext)
  if (!ctx) throw new Error('useToast must be used within a ToastProvider')
  return ctx
}
