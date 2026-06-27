import { useState, useCallback, useEffect } from 'react'

export interface ConfigFile {
  path: string
  content: string
}

/* ══════════════════════════════════════════════════════════════════
 * Action Queue + Debounce (E2E-08)
 *
 * 设计：Editor 侧维护一个 Map<string, string> 待提交变更队列，
 * 每次变更触发 500ms debounce，自动批量调用 commitBatch IPC。
 * 若 Commit 失败，透传 Report JSON 中的错误详情。
 * ══════════════════════════════════════════════════════════════════ */

const COMMIT_DEBOUNCE_MS = 500

// 全局单例 Action Queue（跨组件共享）
const pendingChanges = new Map<string, string>()
let commitTimer: ReturnType<typeof setTimeout> | null = null
let commitCallback: ((success: boolean, report?: string) => void) | null = null

function flushChanges(): Promise<{ success: boolean; report?: string }> {
  if (pendingChanges.size === 0) {
    return Promise.resolve({ success: true })
  }

  // 序列化为 JSON 数组
  const entries: { key: string; value: string }[] = []
  pendingChanges.forEach((value, key) => {
    entries.push({ key, value })
  })
  const entriesJson = JSON.stringify(entries)

  // 发送前清空队列（防止重复提交）
  pendingChanges.clear()

  return window.blessstar.commitBatch(entriesJson).then((res: any) => {
    if (res && res.success) {
      return { success: true, report: res.report }
    }
    return { success: false, report: res?.report }
  })
}

function scheduleCommit(): void {
  if (commitTimer !== null) {
    clearTimeout(commitTimer)
  }
  commitTimer = setTimeout(() => {
    commitTimer = null
    flushChanges().then((result) => {
      if (commitCallback) {
        commitCallback(result.success, result.report)
      }
    })
  }, COMMIT_DEBOUNCE_MS)
}

/** 在 Editor 初始化时调用以注册变更回调 */
export function setCommitCallback(cb: (success: boolean, report?: string) => void): void {
  commitCallback = cb
}

/** 添加一个变更到队列并触发 debounce */
export function addPendingChange(key: string, value: string): void {
  pendingChanges.set(key, value)
  scheduleCommit()
}

/** 立即提交所有待处理变更 */
export function flushPendingChanges(): Promise<{ success: boolean; report?: string }> {
  if (commitTimer !== null) {
    clearTimeout(commitTimer)
    commitTimer = null
  }
  return flushChanges()
}

/** 获取当前待处理变更数 */
export function getPendingChangeCount(): number {
  return pendingChanges.size
}

export function useBlessStar() {
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [pendingCount, setPendingCount] = useState(0)

  // 订阅 commit callback 更新 error state
  useEffect(() => {
    setCommitCallback((success, report) => {
      if (success) {
        setError(null)
      } else {
        setError(`Commit 失败: ${report || '未知错误'}`)
      }
      setPendingCount(getPendingChangeCount())
    })
    return () => {
      setCommitCallback(() => {})
    }
  }, [])

  /** 将字段变更加入 Action Queue */
  const queueChange = useCallback((key: string, value: string) => {
    addPendingChange(key, value)
    setPendingCount(getPendingChangeCount())
  }, [])

  const loadConfig = useCallback(async (): Promise<ConfigFile | null> => {
    setLoading(true)
    setError(null)
    try {
      const result = await window.blessstar.loadConfig()
      if (!result) {
        setLoading(false)
        return null
      }
      const parsed = JSON.parse(result) as ConfigFile
      setLoading(false)
      return parsed
    } catch (err) {
      const msg = err instanceof Error ? err.message : '加载配置失败'
      setError(msg)
      setLoading(false)
      return null
    }
  }, [])

  const saveConfig = useCallback(async (json: string): Promise<boolean> => {
    setLoading(true)
    setError(null)
    try {
      const result = await window.blessstar.saveConfig(json)
      setLoading(false)
      return result
    } catch (err) {
      const msg = err instanceof Error ? err.message : '保存配置失败'
      setError(msg)
      setLoading(false)
      return false
    }
  }, [])

  const saveToPath = useCallback(async (filePath: string, content: string): Promise<boolean> => {
    setLoading(true)
    setError(null)
    try {
      const result = await window.blessstar.saveToPath(filePath, content)
      setLoading(false)
      return result
    } catch (err) {
      const msg = err instanceof Error ? err.message : '保存配置失败'
      setError(msg)
      setLoading(false)
      return false
    }
  }, [])

  const schemaToUidl = useCallback(async (schemaJson?: string): Promise<string | null> => {
    setLoading(true)
    setError(null)
    try {
      const result = await window.blessstar.schemaToUidl(schemaJson)
      setLoading(false)
      return result
    } catch (err) {
      const msg = err instanceof Error ? err.message : '获取 UIDL 失败'
      setError(msg)
      setLoading(false)
      return null
    }
  }, [])

  const clearError = useCallback(() => setError(null), [])

  return {
    loading,
    error,
    pendingCount,
    loadConfig,
    saveConfig,
    saveToPath,
    schemaToUidl,
    clearError,
    queueChange,
    flushPendingChanges: flushPendingChanges,
  }
}

export function useMenuEvents() {
  useEffect(() => {
    const openHandler = window.blessstar.onMenuOpen
    const saveHandler = window.blessstar.onMenuSave
    const saveAsHandler = window.blessstar.onMenuSaveAs

    // These are setup callbacks — calling them registers the handlers
    return () => {
      // cleanup is handled by IPC channel lifecycle
    }
  }, [])
}
