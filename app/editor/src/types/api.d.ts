interface BlessStarAPI {
  loadConfig(): Promise<string | null>
  saveConfig(json: string): Promise<boolean>
  saveToPath(filePath: string, content: string): Promise<boolean>
  schemaToUidl(schemaJson?: string): Promise<string>
  onConfigChanged(callback: (json: string) => void): void
  onMenuOpen(callback: (filePath: string) => void): void
  onMenuSave(callback: () => void): void
  onMenuSaveAs(callback: () => void): void
  // Agent Factory IPC channels (OPT-04)
  exportAgentIndex(config: any): Promise<{ success: boolean; outputDir: string }>
  getRegisteredSchemas(): Promise<any[]>
  getGateChain(): Promise<{ version: string; gates: any[] }>
  validateConfig(configJson: string): Promise<{ valid: boolean; errors: any[] }>
  executeTool(toolName: string, args: unknown): Promise<{ success: boolean; result: string }>
  aiComplete(body: string): Promise<string>
}

interface Window {
  blessstar: BlessStarAPI
}
