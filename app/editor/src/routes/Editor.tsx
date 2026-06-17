import { useState, useCallback, useEffect, useRef } from 'react'
import { useBlessStar } from '../hooks/useBlessStar'
import SchemaForm from '../components/forms/SchemaForm'
import BlocklyWorkspace from '../components/blockly/BlocklyWorkspace'
import { serializeWorkspace, deserializeToWorkspace } from '../components/blockly'
import type { GateChainDocument } from '../components/blockly'
import type { UIDLDocument, FormValues } from '../types/uidl'

type EditorTab = 'schema' | 'rules'

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
  const { loading, loadConfig, saveConfig, saveToPath, schemaToUidl } = useBlessStar()
  const [uidl, setUidl] = useState<UIDLDocument | null>(null)
  const [formValues, setFormValues] = useState<FormValues>({})
  const [currentFilePath, setCurrentFilePath] = useState<string | null>(null)
  const [statusMessage, setStatusMessage] = useState<string | null>(null)
  const [undoStack, setUndoStack] = useState<string[]>([])
  const [redoStack, setRedoStack] = useState<string[]>([])
  const [activeTab, setActiveTab] = useState<EditorTab>('schema')
  const [blocklyJson, setBlocklyJson] = useState<GateChainDocument>(DEMO_GATE_CHAIN)
  const blocklyRef = useRef<{ serialize: () => string } | null>(null)

  // Load initial mock UIDL
  useEffect(() => {
    loadDemoUidl()
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
          setStatusMessage(`已加载: ${config.path}`)
        } catch {
          setStatusMessage('配置文件解析失败')
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
        setStatusMessage('保存成功')
      } else {
        setStatusMessage('保存失败')
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
      setStatusMessage('另存为成功')
    } else {
      setStatusMessage('取消保存')
    }
  }

  const pushUndo = (value: string) => {
    setUndoStack((prev) => [...prev, value])
    setRedoStack([])
  }

  const handleFormChange = useCallback((values: FormValues) => {
    setFormValues(values)
  }, [])

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

  const handleExportGateChain = () => {
    const json = JSON.stringify(blocklyJson, null, 2)
    navigator.clipboard.writeText(json)
    setStatusMessage('Gate 链 JSON 已复制到剪贴板')
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
            {currentFilePath || '未打开文件'}
          </p>
        </div>
        <div className="flex items-center gap-2">
          <button onClick={handleUndo} disabled={undoStack.length === 0} className="btn-ghost" title="撤销">
            ↩
          </button>
          <button onClick={handleRedo} disabled={redoStack.length === 0} className="btn-ghost" title="重做">
            ↪
          </button>
          <div className="w-px h-6 bg-surface-200 dark:bg-surface-700" />
          <button onClick={handleOpenConfig} className="btn-secondary" disabled={loading}>
            打开
          </button>
          <button onClick={handleSave} className="btn-primary" disabled={loading}>
            保存
          </button>
        </div>
      </div>

      {/* Status bar */}
      {statusMessage && (
        <div className="text-xs text-surface-500 dark:text-surface-400 mb-3 px-3 py-1.5 bg-surface-100 dark:bg-surface-800 rounded">
          {statusMessage}
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
      </div>

      {/* Loading state */}
      {loading && (
        <div className="flex items-center justify-center py-12">
          <div className="text-surface-400 dark:text-surface-500">加载中…</div>
        </div>
      )}

      {/* Editor content */}
      {activeTab === 'schema' && uidl && (
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
            />
          </div>
        </div>
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

      {activeTab === 'rules' && (
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
      )}
    </div>
  )
}

export default Editor
