import { useState, useCallback, useEffect } from 'react'

export interface ConfigFile {
  path: string
  content: string
}

export function useBlessStar() {
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)

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

  return { loading, error, loadConfig, saveConfig, saveToPath, schemaToUidl, clearError }
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
