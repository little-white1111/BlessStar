/**
 * ErrorBoundary — React 错误边界组件（第35天 · UX-03）
 *
 * 包裹主要面板（表单/规则编辑器/AI 面板），catch 渲染异常，
 * 显示友好降级 UI 而非白屏或堆栈 trace。
 */

import { Component, type ErrorInfo, type ReactNode } from 'react'

interface Props {
  children: ReactNode
  /** 面板名称，用于降级 UI 中的提示文案 */
  panelName?: string
}

interface State {
  hasError: boolean
}

export class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props)
    this.state = { hasError: false }
  }

  static getDerivedStateFromError(): State {
    return { hasError: true }
  }

  componentDidCatch(error: Error, errorInfo: ErrorInfo) {
    // 仅开发环境记录详细错误，不暴露给用户
    if (typeof window !== 'undefined' && (window as unknown as Record<string, unknown>).__BLESSSTAR_DEV__) {
      console.error('[ErrorBoundary]', error, errorInfo)
    }
  }

  handleRetry = () => {
    this.setState({ hasError: false })
  }

  render() {
    if (this.state.hasError) {
      const name = this.props.panelName || '此面板'
      return (
        <div className="flex items-center justify-center py-12">
          <div className="text-center max-w-sm">
            <p className="text-surface-500 dark:text-surface-400 mb-3">
              {name}遇到意外错误。请尝试刷新页面或联系支持。
            </p>
            <button
              onClick={this.handleRetry}
              className="px-4 py-2 text-sm rounded bg-primary-500 text-white hover:bg-primary-600"
            >
              重试
            </button>
          </div>
        </div>
      )
    }
    return this.props.children
  }
}
