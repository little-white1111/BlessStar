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
})
