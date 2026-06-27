interface BlessStarAPI {
  loadConfig(): Promise<string | null>
  saveConfig(json: string): Promise<boolean>
  saveToPath(filePath: string, content: string): Promise<boolean>
  schemaToUidl(schemaJson?: string): Promise<string>
  onConfigChanged(callback: (json: string) => void): void
  onMenuOpen(callback: (filePath: string) => void): void
  onMenuSave(callback: () => void): void
  onMenuSaveAs(callback: () => void): void
  exportAgentIndex(config: any): Promise<{ success: boolean; outputDir: string }>
  getRegisteredSchemas(): Promise<any[]>
  getGateChain(): Promise<{ version: string; gates: any[] }>
  validateConfig(configJson: string): Promise<{ valid: boolean; errors: any[] }>
  executeTool(toolName: string, args: unknown): Promise<{ success: boolean; result: string }>
  aiComplete(body: string): Promise<string>
  /** Ollama 模型列表 */
  ollamaListModels(): Promise<{ name: string; modified_at: string; size: number }[]>
  /** 云端 AI 聊天（OpenAI 兼容接口：DeepSeek / OpenAI） */
  aiChat(config: { baseUrl: string; apiKey: string; model: string; body: string }): Promise<string>
  /** EMB: Embedding API（OpenAI 兼容 & Ollama） */
  aiEmbed(config: { url: string; apiKey?: string; body: string }): Promise<string>

  /* ══════════════════════════════════════════════════════════════════
   * E2E Editor Bridge — 7 new channels (2026-06-19)
   * ══════════════════════════════════════════════════════════════════ */
  /** normalizeVendor: 厂商配置归一化（通过 NormalizerRegistry 分发） */
  normalizeVendor(vendorId: string, inputJson: string, extraJson?: string): Promise<{ success: boolean; result: string | null }>
  /** appSessionCreate: 创建 AppSession */
  appSessionCreate(): Promise<{ success: boolean; handle: number | null }>
  /** appSessionDestroy: 销毁 AppSession */
  appSessionDestroy(): Promise<{ success: boolean }>
  /** commitBatch: 批量提交配置变更 */
  commitBatch(entriesJson: string): Promise<{ success: boolean; report?: string; error?: string }>
  /** registerGate: 注册 Gate 规则（第34天 · GR-01） */
  registerGate(gateType: string, ruleJson: string): Promise<{ success: boolean; error?: string }>
  /** subscribeWatch: 订阅配置变更 Watch 事件 */
  subscribeWatch(): Promise<{ success: boolean }>

  /* ══════════════════════════════════════════════════════════════════
   * SafeStorage: API Key 加密存储（第33天）
   * ══════════════════════════════════════════════════════════════════ */

  /** encryptString: 使用 Electron safeStorage 加密字符串 */
  encryptString(plaintext: string): Promise<string>

  /** decryptString: 使用 Electron safeStorage 解密字符串 */
  decryptString(ciphertext: string): Promise<string>

  /** isEncryptionAvailable: 检查 safeStorage 是否可用 */
  isEncryptionAvailable(): Promise<boolean>

  /* ══════════════════════════════════════════════════════════════════
   * 版本注册表持久化（第33天 · RV-03 元数据与配置分离）
   * ══════════════════════════════════════════════════════════════════ */

  /** loadVersionRegistry: 加载版本注册表 */
  loadVersionRegistry(): Promise<string | null>

  /** saveVersionRegistry: 保存版本注册表 */
  saveVersionRegistry(json: string): Promise<boolean>
}

interface Window {
  blessstar: BlessStarAPI
}
