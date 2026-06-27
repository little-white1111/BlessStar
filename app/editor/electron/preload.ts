import { contextBridge, ipcRenderer } from 'electron'

contextBridge.exposeInMainWorld('blessstar', {
  loadConfig: (): Promise<string | null> => {
    return ipcRenderer.invoke('blessstar:loadConfig')
  },

  saveConfig: (json: string): Promise<boolean> => {
    return ipcRenderer.invoke('blessstar:saveConfig', json)
  },

  saveToPath: (filePath: string, content: string): Promise<boolean> => {
    return ipcRenderer.invoke('blessstar:saveToPath', filePath, content)
  },

  schemaToUidl: (schemaJson?: string): Promise<string> => {
    return ipcRenderer.invoke('blessstar:schemaToUidl', schemaJson)
  },

  onConfigChanged: (callback: (json: string) => void): void => {
    const handler = (_event: Electron.IpcRendererEvent, json: string) => {
      callback(json)
      // Auto-refresh agent index on config change
      ipcRenderer.invoke('blessstar:exportAgentIndex', {
        outputDir: '.cursor/agents/',
        businessName: 'default',
        includeAiHints: true,
        includeGateChain: true
      }).then(result => {
        console.log('[AgentFactory] Index auto-refreshed:', result.outputDir)
      }).catch(err => {
        console.error('[AgentFactory] Auto-refresh failed:', err)
      })
    }
    ipcRenderer.on('config:changed', handler)
  },

  onMenuOpen: (callback: (filePath: string) => void): void => {
    const handler = (_event: Electron.IpcRendererEvent, filePath: string) => {
      callback(filePath)
    }
    ipcRenderer.on('menu:open', handler)
  },

  onMenuSave: (callback: () => void): void => {
    ipcRenderer.on('menu:save', () => callback())
  },

  onMenuSaveAs: (callback: () => void): void => {
    ipcRenderer.on('menu:saveAs', () => callback())
  },

  // Agent Factory IPC channels (OPT-04)
  exportAgentIndex: (config: any) => {
    return ipcRenderer.invoke('blessstar:exportAgentIndex', config)
  },

  getRegisteredSchemas: () => {
    return ipcRenderer.invoke('blessstar:getRegisteredSchemas')
  },

  getGateChain: () => {
    return ipcRenderer.invoke('blessstar:getGateChain')
  },

  validateConfig: (configJson: string) => {
    return ipcRenderer.invoke('blessstar:validateConfig', configJson)
  },

  executeTool: (toolName: string, args: unknown) => {
    return ipcRenderer.invoke('blessstar:executeTool', toolName, args)
  },

  // Ollama AI completion via main process
  aiComplete: (body: string): Promise<string> => {
    return ipcRenderer.invoke('blessstar:aiComplete', body)
  },

  // Ollama 模型列表
  ollamaListModels: (): Promise<{ name: string; modified_at: string; size: number }[]> => {
    return ipcRenderer.invoke('blessstar:ollamaListModels')
  },

  // 云端 AI 聊天（OpenAI 兼容接口：DeepSeek / OpenAI）
  aiChat: (config: { baseUrl: string; apiKey: string; model: string; body: string }): Promise<string> => {
    return ipcRenderer.invoke('blessstar:aiChat', config)
  },

  // EMB: Embedding API（OpenAI 兼容 & Ollama）
  aiEmbed: (config: { url: string; apiKey?: string; body: string }): Promise<string> => {
    return ipcRenderer.invoke('blessstar:aiEmbed', config)
  },

  /* ══════════════════════════════════════════════════════════════════
   * E2E Editor Bridge — 7 new channels (2026-06-19)
   * ══════════════════════════════════════════════════════════════════ */

  /** normalizeVendor: 厂商配置归一化（通过 NormalizerRegistry 分发） */
  normalizeVendor: (vendorId: string, inputJson: string, extraJson?: string) => {
    return ipcRenderer.invoke('blessstar:normalizeVendor', vendorId, inputJson, extraJson)
  },

  /** appSessionCreate: 创建 AppSession */
  appSessionCreate: () => {
    return ipcRenderer.invoke('blessstar:appSessionCreate')
  },

  /** appSessionDestroy: 销毁 AppSession */
  appSessionDestroy: () => {
    return ipcRenderer.invoke('blessstar:appSessionDestroy')
  },

  /** commitBatch: 批量提交配置变更 */
  commitBatch: (entriesJson: string) => {
    return ipcRenderer.invoke('blessstar:commitBatch', entriesJson)
  },

  /** registerGate: 注册 Gate 规则（第34天 · GR-01：Gate 是基础设施） */
  registerGate: (gateType: string, ruleJson: string) => {
    return ipcRenderer.invoke('blessstar:registerGate', gateType, ruleJson)
  },

  /** subscribeWatch: 订阅配置变更 Watch 事件 */
  subscribeWatch: () => {
    return ipcRenderer.invoke('blessstar:subscribeWatch')
  },

  /* ══════════════════════════════════════════════════════════════════
   * SafeStorage: API Key 加密存储（第33天）
   * ══════════════════════════════════════════════════════════════════ */

  /** encryptString: 使用 Electron safeStorage 加密字符串 */
  encryptString: (plaintext: string): Promise<string> => {
    return ipcRenderer.invoke('blessstar:encryptString', plaintext)
  },

  /** decryptString: 使用 Electron safeStorage 解密字符串 */
  decryptString: (ciphertext: string): Promise<string> => {
    return ipcRenderer.invoke('blessstar:decryptString', ciphertext)
  },

  /** isEncryptionAvailable: 检查 safeStorage 是否可用 */
  isEncryptionAvailable: (): Promise<boolean> => {
    return ipcRenderer.invoke('blessstar:isEncryptionAvailable')
  },

  /* ══════════════════════════════════════════════════════════════════
   * 版本注册表持久化（第33天 · RV-03 元数据与配置分离）
   * ══════════════════════════════════════════════════════════════════ */

  /** loadVersionRegistry: 加载版本注册表 */
  loadVersionRegistry: (): Promise<string | null> => {
    return ipcRenderer.invoke('blessstar:loadVersionRegistry')
  },

  /** saveVersionRegistry: 保存版本注册表 */
  saveVersionRegistry: (json: string): Promise<boolean> => {
    return ipcRenderer.invoke('blessstar:saveVersionRegistry', json)
  },
})
