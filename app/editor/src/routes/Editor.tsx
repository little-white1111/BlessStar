import { useState, useCallback, useEffect, useRef } from 'react'
import { useBlessStar } from '../hooks/useBlessStar'
import { initDynamicDomains } from '../ai/context-manager/indexShardLoader'
import { loadVersionRegistry, renameVersion } from '../ai/versionRegistry'
import type { VersionRegistry, ConfigVersion } from '../ai/types'
import SchemaForm from '../components/forms/SchemaForm'
import BlocklyWorkspace from '../components/blockly/BlocklyWorkspace'
import { serializeWorkspace, deserializeToWorkspace } from '../components/blockly'
import type { GateChainDocument } from '../components/blockly'
import type { UIDLDocument, FormValues } from '../types/uidl'
// ── 第35天 UX 组件 ──────────────────────────────────
import ProjectManager, { type ProjectMeta } from '../components/ProjectManager'
import { useToast } from '../components/ToastProvider'
import { ErrorBoundary } from '../components/ErrorBoundary'
import {
  serializeProjectExport,
  deserializeProjectImport,
  type ProjectExportData,
} from '../components/ProjectExport'

type EditorTab = 'schema' | 'rules' | 'dashboard'

const DEMO_GATE_CHAIN: GateChainDocument = {
  version: '1.0',
  gates: [
    {
      type: 'gate_default',
      gate_id: 'main_gate',
      scenario: 'default',
      do: [
        {
          type: 'condition',
          condition_id: 'check_env',
          if: {
            type: 'meta_rule',
            field: 'environment',
            operator: 'eq',
            value: 'production',
          },
          then: [
            {
              type: 'meta_rule',
              field: 'audit.enabled',
              operator: 'eq',
              value: 'true',
            },
          ],
          else: [
            {
              type: 'policy_attr',
              key: 'mode',
              value: 'debug',
            },
          ],
        },
      ],
    },
  ],
}

function Editor() {
  const { loading, loadConfig, saveConfig, saveToPath, schemaToUidl, queueChange, pendingCount } = useBlessStar()
  const toast = useToast()
  const [uidl, setUidl] = useState<UIDLDocument | null>(null)
  const [formValues, setFormValues] = useState<FormValues>({})
  const [currentFilePath, setCurrentFilePath] = useState<string | null>(null)
  const [statusMessage, setStatusMessage] = useState<string | null>(null)
  const [metrics, setMetrics] = useState<{ walOps?: number; gateEvals?: number; configReads?: number; errors?: number } | null>(null)
  const [undoStack, setUndoStack] = useState<string[]>([])
  const [redoStack, setRedoStack] = useState<string[]>([])
  const [activeTab, setActiveTab] = useState<EditorTab>('schema')
  const [blocklyJson, setBlocklyJson] = useState<GateChainDocument>(DEMO_GATE_CHAIN)
  const blocklyRef = useRef<{ serialize: () => string } | null>(null)
  // ── 第35天 · 多项目管理 ───────────────────────────
  const [currentProject, setCurrentProject] = useState<string | null>(null)
  const [isNewProjectOpen, setIsNewProjectOpen] = useState(false)
  const [newProjectNameInput, setNewProjectNameInput] = useState('')

  // ── 版本注册表（第33天 · 仪表盘 + 配置编辑器版本下拉）─────────
  const [versionRegistry, setVersionRegistry] = useState<VersionRegistry>({})
  const [renamingVersion, setRenamingVersion] = useState<{ key: string; versionId: string } | null>(null)
  const [renameInput, setRenameInput] = useState('')

  const refreshVersionRegistry = useCallback(async () => {
    try {
      const reg = await loadVersionRegistry()
      setVersionRegistry(reg)
    } catch {
      // 加载失败，使用空注册表
    }
  }, [])

  useEffect(() => {
    refreshVersionRegistry()
  }, [refreshVersionRegistry])

  const handleVersionRename = useCallback(async (configKey: string, versionId: string, newName: string) => {
    await renameVersion(configKey, versionId, newName)
    await refreshVersionRegistry()
    setRenamingVersion(null)
    setRenameInput('')
  }, [refreshVersionRegistry])

  // Load initial mock UIDL
  useEffect(() => {
    loadDemoUidl()
    // 加载动态 Domain 索引（E2E-05）
    initDynamicDomains()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  // Register menu event handlers
  useEffect(() => {
    window.blessstar.onMenuOpen(async (filePath: string) => {
      try {
        const resp = await fetch(`file://${filePath}`)
        handleOpenConfig()
      } catch {
        handleOpenConfig()
      }
    })

    window.blessstar.onMenuSave(() => {
      handleSave()
    })

    window.blessstar.onMenuSaveAs(() => {
      handleSaveAs()
    })
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [formValues, currentFilePath])

  // AI 写入配置后实时刷新表单（单独 effect，避免 repeated re-subscribe）
  useEffect(() => {
    window.blessstar.onConfigChanged((json: string) => {
      try {
        const data = JSON.parse(json)
        // 单 key 写入：{"key":"...", "value":"..."}
        if (data && typeof data.key === 'string' && data.value !== undefined) {
          setFormValues(prev => ({ ...prev, [data.key]: data.value }))
        }
      } catch {
        // 非 JSON 格式（如完整配置文件保存），忽略
      }
      // 一并刷新版本注册表
      refreshVersionRegistry()
    })
  }, []) // eslint-disable-line react-hooks/exhaustive-deps

  const loadDemoUidl = async () => {
    const result = await schemaToUidl()
    if (result) {
      try {
        const doc = JSON.parse(result) as UIDLDocument
        setUidl(doc)
        setStatusMessage('已加载演示配置模板')
      } catch {
        setStatusMessage('UIDL 解析失败')
      }
    }
  }

  const handleOpenConfig = async () => {
    const config = await loadConfig()
    if (config) {
      setCurrentFilePath(config.path)
      const uidlResult = await schemaToUidl(config.content)
      if (uidlResult) {
        try {
          const doc = JSON.parse(uidlResult) as UIDLDocument
          setUidl(doc)
          toast.showToast('success', `已加载: ${config.path}`)
        } catch {
          toast.showToast('error', '配置文件解析失败')
        }
      }
    }
  }

  const handleSave = async () => {
    if (currentFilePath) {
      const jsonContent = JSON.stringify(formValues, null, 2)
      const success = await saveToPath(currentFilePath, jsonContent)
      if (success) {
        pushUndo(jsonContent)
        toast.showToast('success', '保存成功')
      } else {
        toast.showToast('error', '保存失败')
      }
    } else {
      handleSaveAs()
    }
  }

  const handleSaveAs = async () => {
    const jsonContent = JSON.stringify(formValues, null, 2)
    const success = await saveConfig(jsonContent)
    if (success) {
      pushUndo(jsonContent)
      toast.showToast('success', '另存为成功')
    } else {
      toast.showToast('warning', '已取消保存')
    }
  }

  const pushUndo = (value: string) => {
    setUndoStack((prev) => [...prev, value])
    setRedoStack([])
  }

  const handleFormChange = useCallback((values: FormValues) => {
    setFormValues(values)
    // 将每个字段变更加入 Action Queue（E2E-08）
    for (const [key, value] of Object.entries(values)) {
      if (typeof value === 'string') {
        queueChange(key, value)
      }
    }
  }, [queueChange])

  const handleUndo = () => {
    if (undoStack.length > 0) {
      const prev = undoStack[undoStack.length - 1]
      setUndoStack((prev) => prev.slice(0, -1))
      setRedoStack((prev) => [...prev, JSON.stringify(formValues)])
      try {
        const parsed = JSON.parse(prev)
        setFormValues(parsed)
      } catch {
        // ignore
      }
    }
  }

  const handleRedo = () => {
    if (redoStack.length > 0) {
      const next = redoStack[redoStack.length - 1]
      setRedoStack((prev) => prev.slice(0, -1))
      setUndoStack((prev) => [...prev, JSON.stringify(formValues)])
      try {
        const parsed = JSON.parse(next)
        setFormValues(parsed)
      } catch {
        // ignore
      }
    }
  }

  const handleBlocklyChange = useCallback((json: GateChainDocument) => {
    setBlocklyJson(json)
  }, [])

  // ── 第35天 · 快捷键注册（Ctrl+S / Ctrl+Z / Ctrl+Shift+N）─────────
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const ctrl = e.ctrlKey || e.metaKey
      if (ctrl && e.key === 's') {
        e.preventDefault()
        handleSave()
      } else if (ctrl && e.key === 'z' && !e.shiftKey) {
        e.preventDefault()
        handleUndo()
      } else if (ctrl && e.shiftKey && e.key === 'N') {
        e.preventDefault()
        setNewProjectNameInput('')
        setIsNewProjectOpen(true)
      }
    }
    window.addEventListener('keydown', handler)
    return () => window.removeEventListener('keydown', handler)
  }, [handleSave, handleUndo])

  // ── 第35天 · 多项目管理回调 ───────────────────────────
  const handleOpenProject = useCallback((p: ProjectMeta) => {
    setCurrentProject(p.name)
    toast.showToast('success', `已打开项目：${p.name}`)
  }, [toast])

  const handleNewProject = useCallback(async (name: string): Promise<ProjectMeta | null> => {
    const now = new Date().toISOString()
    const meta: ProjectMeta = { name, createdAt: now, lastOpened: now, path: name }
    setCurrentProject(name)
    toast.showToast('success', `项目「${name}」已创建`)
    setIsNewProjectOpen(false)
    setNewProjectNameInput('')
    return meta
  }, [toast])

  const handleCloseProject = useCallback(() => {
    setCurrentProject(null)
    setUidl(null)
    setFormValues({})
    setBlocklyJson(DEMO_GATE_CHAIN)
    toast.showToast('success', '项目已关闭')
  }, [toast])

  // ── 第35天 · 项目导出 ────────────────────────────────
  const handleExportProject = useCallback(async () => {
    const exportData: ProjectExportData = {
      formatVersion: '1.0',
      exportedAt: new Date().toISOString(),
      project: {
        name: currentProject || 'untitled',
        createdAt: new Date().toISOString(),
        lastOpened: new Date().toISOString(),
      },
      schema: uidl,
      gateChains: blocklyJson,
      config: formValues,
    }
    const json = serializeProjectExport(exportData)
    try {
      await navigator.clipboard.writeText(json)
      toast.showToast('success', '项目已导出并复制到剪贴板（也可通过 Ctrl+S 保存到文件）')
    } catch {
      const blob = new Blob([json], { type: 'application/json' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `${currentProject || 'project'}.bsproject.json`
      a.click()
      URL.revokeObjectURL(url)
      toast.showToast('success', `项目已导出为 ${a.download}`)
    }
  }, [currentProject, uidl, blocklyJson, formValues, toast])

  // ── 第35天 · 项目导入 ────────────────────────────────
  const handleImportProject = useCallback(() => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.json,.bsproject.json'
    input.onchange = async (e) => {
      const file = (e.target as HTMLInputElement).files?.[0]
      if (!file) return
      const text = await file.text()
      const result = deserializeProjectImport(text)
      if (!result.valid) {
        toast.showToast('error', `导入失败: ${result.error}`)
        return
      }
      const data = result.data!
      // 恢复项目数据
      if (data.schema) setUidl(data.schema as UIDLDocument)
      if (data.config) setFormValues(data.config as FormValues)
      if (data.gateChains) setBlocklyJson(data.gateChains as GateChainDocument)
      setCurrentProject(data.project.name)
      toast.showToast('success', `已导入项目：${data.project.name}`)
    }
    input.click()
  }, [toast])

  const handleExportGateChain = () => {
    const json = JSON.stringify(blocklyJson, null, 2)
    navigator.clipboard.writeText(json)
    toast.showToast('success', 'Gate 链 JSON 已复制到剪贴板')
  }

  return (
    <div className="h-full flex flex-col">
      {/* Toolbar */}
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-2xl font-bold text-surface-900 dark:text-surface-50">
            配置编辑器
          </h1>
          <p className="text-sm text-surface-500 dark:text-surface-400 mt-1">
            {currentProject || (currentFilePath || '未打开文件')}
          </p>
        </div>
        <div className="flex items-center gap-2 relative">
          {/* 项目管理下拉（第35天 · 多项目）*/}
          <ProjectManager
            currentProject={currentProject}
            onOpenProject={handleOpenProject}
            onNewProject={handleNewProject}
            onCloseProject={handleCloseProject}
          />
          <button onClick={handleUndo} disabled={undoStack.length === 0} className="btn-ghost text-sm px-2" title="撤销 (Ctrl+Z)">
            ↩
          </button>
          <button onClick={handleRedo} disabled={redoStack.length === 0} className="btn-ghost text-sm px-2" title="重做">
            ↪
          </button>
          <div className="w-px h-6 bg-surface-200 dark:bg-surface-700" />
          <button onClick={handleOpenConfig} className="btn-secondary text-sm" disabled={loading}>
            打开
          </button>
          <button onClick={handleSave} className="btn-primary text-sm" disabled={loading}>
            保存
          </button>
          {/* ── 第35天 · 导出/导入 ────────────────── */}
          <div className="w-px h-6 bg-surface-200 dark:bg-surface-700" />
          <button onClick={handleExportProject} className="btn-secondary text-sm" title="导出项目">
            ↗ 导出
          </button>
          <button onClick={handleImportProject} className="btn-secondary text-sm" title="导入项目">
            ↙ 导入
          </button>
        </div>
      </div>

      {/* Status bar — DAY38-04: 4 项实时监控指标 */}
      {(statusMessage || pendingCount > 0 || metrics) && (
        <div className="text-xs text-surface-500 dark:text-surface-400 mb-3 px-3 py-1.5 bg-surface-100 dark:bg-surface-800 rounded flex items-center gap-3 flex-wrap">
          {statusMessage && <span>{statusMessage}</span>}
          {pendingCount > 0 && (
            <span className="text-amber-600 dark:text-amber-400 font-medium">
              {pendingCount} 个待提交变更…
            </span>
          )}
          {/* WAL ops */}
          {metrics && metrics.walOps !== undefined && (
            <span title="WAL 写入操作数">WAL: {metrics.walOps}</span>
          )}
          {/* Gate evals */}
          {metrics && metrics.gateEvals !== undefined && (
            <span title="Gate 评估次数">Gate: {metrics.gateEvals}</span>
          )}
          {/* Config reads */}
          {metrics && metrics.configReads !== undefined && (
            <span>Read: {metrics.configReads}</span>
          )}
          {/* Errors */}
          {metrics && metrics.errors !== undefined && metrics.errors > 0 && (
            <span className="text-red-500 font-medium" title="错误数超阈值">
              ⚠ Err: {metrics.errors}
            </span>
          )}
        </div>
      )}

      {/* Tab switcher */}
      <div className="flex gap-1 mb-4 border-b border-surface-200 dark:border-surface-700">
        <button
          onClick={() => setActiveTab('schema')}
          className={`px-4 py-2 text-sm font-medium rounded-t transition-colors ${
            activeTab === 'schema'
              ? 'text-primary-600 dark:text-primary-400 border-b-2 border-primary-600 dark:border-primary-400'
              : 'text-surface-500 dark:text-surface-400 hover:text-surface-700 dark:hover:text-surface-200'
          }`}
        >
          Schema 表单
        </button>
        <button
          onClick={() => setActiveTab('rules')}
          className={`px-4 py-2 text-sm font-medium rounded-t transition-colors ${
            activeTab === 'rules'
              ? 'text-primary-600 dark:text-primary-400 border-b-2 border-primary-600 dark:border-primary-400'
              : 'text-surface-500 dark:text-surface-400 hover:text-surface-700 dark:hover:text-surface-200'
          }`}
        >
          规则编辑器 (积木)
        </button>
        <button
          onClick={() => { setActiveTab('dashboard'); refreshVersionRegistry() }}
          className={`px-4 py-2 text-sm font-medium rounded-t transition-colors ${
            activeTab === 'dashboard'
              ? 'text-primary-600 dark:text-primary-400 border-b-2 border-primary-600 dark:border-primary-400'
              : 'text-surface-500 dark:text-surface-400 hover:text-surface-700 dark:hover:text-surface-200'
          }`}
        >
          仪表盘
        </button>
      </div>

      {/* Loading skeleton (第35天 · 骨架屏) */}
      {loading && (
        <div className="flex-1 overflow-y-auto animate-pulse">
          <div className="card p-6 space-y-4">
            <div className="h-6 bg-surface-200 dark:bg-surface-700 rounded w-1/3" />
            <div className="h-4 bg-surface-200 dark:bg-surface-700 rounded w-2/3" />
            <div className="space-y-3 mt-6">
              {[1, 2, 3, 4].map((i) => (
                <div key={i} className="space-y-2">
                  <div className="h-4 bg-surface-200 dark:bg-surface-700 rounded w-1/4" />
                  <div className="h-10 bg-surface-200 dark:bg-surface-700 rounded" />
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* Editor content */}
      {activeTab === 'schema' && uidl && !loading && (
        <ErrorBoundary panelName="Schema 表单">
          <div className="flex-1 overflow-y-auto">
            <div className="card p-6">
              <div className="mb-6">
                <h2 className="text-xl font-semibold text-surface-900 dark:text-surface-50">
                  {uidl.title}
                </h2>
                {uidl.description && (
                  <p className="text-sm text-surface-500 dark:text-surface-400 mt-1">
                    {uidl.description}
                  </p>
                )}
              </div>
              <SchemaForm
                fields={uidl.fields}
                values={formValues}
                onChange={handleFormChange}
                versionRegistry={versionRegistry}
              />
            </div>
          </div>
        </ErrorBoundary>
      )}

      {activeTab === 'schema' && !uidl && !loading && (
        <div className="flex-1 flex items-center justify-center">
          <div className="text-center">
            <p className="text-surface-400 dark:text-surface-500 mb-4">
              点击「打开」加载配置文件，或使用演示模板
            </p>
            <button onClick={loadDemoUidl} className="btn-secondary">
              加载演示模板
            </button>
          </div>
        </div>
      )}

      {activeTab === 'rules' && !loading && (
        <ErrorBoundary panelName="规则编辑器">
          <div className="flex-1 flex flex-col gap-4">
            {/* Blockly workspace */}
            <div className="flex-1 card overflow-hidden" style={{ minHeight: '450px' }}>
              <div className="h-full w-full">
                <BlocklyWorkspace
                  initialJson={blocklyJson}
                  onChange={handleBlocklyChange}
                  maxDepth={3}
                />
              </div>
            </div>

            {/* Gate chain toolbar */}
            <div className="flex items-center gap-2">
              <button
                onClick={handleExportGateChain}
                className="btn-secondary text-sm"
              >
                复制 Gate 链 JSON
              </button>
              <span className="text-xs text-surface-400 dark:text-surface-500">
                嵌套深度 ≤3 层 | 双向序列化 (积木 ↔ Gate 链 JSON)
              </span>
            </div>
          </div>
        </ErrorBoundary>
      )}

      {/* ── 仪表盘 Tab（第33天 · 版本活动记录）─────────────────── */}
      {activeTab === 'dashboard' && !loading && (
        <ErrorBoundary panelName="仪表盘">
          <div className="flex-1 overflow-y-auto">
            <div className="card p-6">
            <h2 className="text-lg font-semibold mb-4 text-surface-900 dark:text-surface-50">
              版本活动记录
            </h2>
            {Object.keys(versionRegistry).length === 0 ? (
              <div className="text-sm text-surface-400 dark:text-surface-500 text-center py-8">
                暂无活动记录。通过 AI 助手修改配置后将在此显示。
              </div>
            ) : (
              <div className="space-y-4">
                {Object.entries(versionRegistry).flatMap(([configKey, versions]) =>
                  versions.map((v: ConfigVersion) => (
                    <div
                      key={v.versionId}
                      className="flex items-center justify-between py-2 px-3 rounded border border-surface-200 dark:border-surface-700"
                    >
                      <div className="flex-1 min-w-0">
                        <div className="flex items-center gap-2">
                          <span className="text-xs font-mono text-surface-400 dark:text-surface-500">
                            {new Date(v.timestamp).toLocaleString('zh-CN')}
                          </span>
                          <span className="text-xs px-1.5 py-0.5 rounded bg-primary-50 dark:bg-primary-900/20 text-primary-600 dark:text-primary-400">
                            配置变更
                          </span>
                        </div>
                        <div className="mt-1 text-sm text-surface-700 dark:text-surface-300 truncate">
                          {configKey} → {v.value}
                        </div>
                        <div className="text-xs text-surface-400 dark:text-surface-500 mt-0.5 flex items-center gap-2">
                          <span>版本: {v.displayName || v.versionId}</span>
                          <button
                            onClick={() => { setRenamingVersion({ key: configKey, versionId: v.versionId }); setRenameInput(v.displayName) }}
                            className="hover:text-primary-500"
                          >
                            ✏
                          </button>
                        </div>
                        {v.userInput && (
                          <div className="text-xs text-surface-400 dark:text-surface-500 mt-0.5">
                            对话: "{v.userInput}"
                          </div>
                        )}
                      </div>
                    </div>
                  ))
                )}
              </div>
            )}

            {/* 版本重命名弹窗 */}
            {renamingVersion && (
              <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/30">
                <div className="bg-white dark:bg-surface-800 rounded-lg p-4 shadow-xl min-w-[320px]">
                  <h3 className="text-sm font-semibold mb-3 text-surface-900 dark:text-surface-50">
                    重命名版本
                  </h3>
                  <input
                    autoFocus
                    value={renameInput}
                    onChange={(e) => setRenameInput(e.target.value)}
                    placeholder="输入版本名称（如：节日房间号）"
                    className="w-full px-3 py-2 text-sm rounded border border-surface-300 dark:border-surface-600 bg-white dark:bg-surface-700 text-surface-900 dark:text-surface-50 mb-3"
                  />
                  <div className="flex justify-end gap-2">
                    <button
                      onClick={() => setRenamingVersion(null)}
                      className="px-3 py-1.5 text-sm rounded border border-surface-300 dark:border-surface-600 text-surface-600 dark:text-surface-400"
                    >
                      取消
                    </button>
                    <button
                      onClick={() => handleVersionRename(renamingVersion.key, renamingVersion.versionId, renameInput)}
                      className="px-3 py-1.5 text-sm rounded bg-primary-500 text-white"
                    >
                      确认
                    </button>
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>
        </ErrorBoundary>
      )}

      {/* ── 第35天 · 新建项目弹窗（Ctrl+Shift+N）───────────────── */}
      {isNewProjectOpen && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/30">
          <div className="bg-white dark:bg-surface-800 rounded-lg p-6 shadow-xl min-w-[360px]">
            <h3 className="text-lg font-semibold mb-4 text-surface-900 dark:text-surface-50">
              新建项目
            </h3>
            <input
              autoFocus
              value={newProjectNameInput}
              onChange={(e) => setNewProjectNameInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && newProjectNameInput.trim()) {
                  handleNewProject(newProjectNameInput.trim())
                }
                if (e.key === 'Escape') {
                  setIsNewProjectOpen(false)
                  setNewProjectNameInput('')
                }
              }}
              placeholder="输入项目名称（如：游戏房间号配置）"
              className="w-full px-3 py-2 text-sm rounded border border-surface-300 dark:border-surface-600 bg-white dark:bg-surface-700 text-surface-900 dark:text-surface-50 mb-4"
            />
            <div className="flex justify-end gap-2">
              <button
                onClick={() => { setIsNewProjectOpen(false); setNewProjectNameInput('') }}
                className="px-3 py-1.5 text-sm rounded border border-surface-300 dark:border-surface-600 text-surface-600 dark:text-surface-400"
              >
                取消
              </button>
              <button
                onClick={() => {
                  if (newProjectNameInput.trim()) {
                    handleNewProject(newProjectNameInput.trim())
                  }
                }}
                disabled={!newProjectNameInput.trim()}
                className="px-4 py-1.5 text-sm rounded bg-primary-500 text-white hover:bg-primary-600 disabled:opacity-40"
              >
                创建
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}

export default Editor
