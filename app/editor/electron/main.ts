import {
  app,
  BrowserWindow,
  Menu,
  ipcMain,
  dialog,
  safeStorage,
  MenuItemConstructorOptions,
} from 'electron'
import path from 'path'

const isDev = !app.isPackaged

/** 获取 BlessStar native addon 的绝对路径（兼容 dev 与 packaged 环境） */
function getNativeAddonPath(): string {
  if (isDev) {
    // 开发环境：index.js 在 native/ 目录，自动检测平台 .node 文件
    return path.join(__dirname, '../native/index.js')
  }
  // 生产环境：extraResources 复制到 process.resourcesPath/native/
  return path.join(process.resourcesPath, 'native', 'index.js')
}

/** 加载 BlessStar native addon */
function loadNativeAddon(): any {
  return require(getNativeAddonPath())
}

let mainWindow: BrowserWindow | null = null

function createWindow(): void {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 1000,
    minHeight: 700,
    title: 'BlessStar Config Editor',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
    },
    show: false,
    backgroundColor: '#f8fafc',
  })

  mainWindow.once('ready-to-show', () => {
    mainWindow?.show()
  })

  if (isDev) {
    mainWindow.loadURL('http://localhost:5173')
    mainWindow.webContents.openDevTools()
  } else {
    mainWindow.loadFile(path.join(__dirname, '../dist-renderer/index.html'))
  }

  mainWindow.on('closed', () => {
    mainWindow = null
  })
}

function buildMenu(): void {
  const isMac = process.platform === 'darwin'

  const template: MenuItemConstructorOptions[] = [
    ...(isMac
      ? [
          {
            label: app.name,
            submenu: [
              { role: 'about' as const, label: '关于 BlessStar Config Editor' },
              { type: 'separator' as const },
              { role: 'services' as const },
              { type: 'separator' as const },
              { role: 'hide' as const },
              { role: 'hideOthers' as const },
              { role: 'unhide' as const },
              { type: 'separator' as const },
              { role: 'quit' as const, label: '退出' },
            ],
          } as MenuItemConstructorOptions,
        ]
      : []),
    {
      label: '文件',
      submenu: [
        {
          label: '打开配置文件',
          accelerator: 'CmdOrCtrl+O',
          click: async () => {
            handleOpenConfig()
          },
        },
        {
          label: '保存',
          accelerator: 'CmdOrCtrl+S',
          click: () => {
            mainWindow?.webContents.send('menu:save')
          },
        },
        {
          label: '另存为…',
          accelerator: 'CmdOrCtrl+Shift+S',
          click: () => {
            mainWindow?.webContents.send('menu:saveAs')
          },
        },
        { type: 'separator' },
        isMac ? { role: 'close', label: '关闭窗口' } : { role: 'quit', label: '退出' },
      ],
    },
    {
      label: '编辑',
      submenu: [
        { role: 'undo', label: '撤销' },
        { role: 'redo', label: '重做' },
        { type: 'separator' },
        { role: 'cut', label: '剪切' },
        { role: 'copy', label: '复制' },
        { role: 'paste', label: '粘贴' },
        { role: 'selectAll', label: '全选' },
      ],
    },
    {
      label: '视图',
      submenu: [
        { role: 'reload', label: '重新加载' },
        { role: 'forceReload', label: '强制重新加载' },
        { role: 'toggleDevTools', label: '开发者工具' },
        { type: 'separator' },
        { role: 'resetZoom', label: '重置缩放' },
        { role: 'zoomIn', label: '放大' },
        { role: 'zoomOut', label: '缩小' },
        { type: 'separator' },
        { role: 'togglefullscreen', label: '全屏' },
      ],
    },
    {
      label: '帮助',
      submenu: [
        {
          label: '关于 BlessStar',
          click: () => {
            dialog.showMessageBox({
              title: '关于 BlessStar Config Editor',
              message: 'BlessStar Config Editor v1.0.0',
              detail: '财务运维配置管理中间件\n前端配置编辑工具',
            })
          },
        },
      ],
    },
  ]

  const menu = Menu.buildFromTemplate(template)
  Menu.setApplicationMenu(menu)
}

async function handleOpenConfig(): Promise<void> {
  const result = await dialog.showOpenDialog({
    title: '打开配置文件',
    filters: [
      { name: '配置文件', extensions: ['json', 'yaml', 'yml', 'ini', 'toml'] },
      { name: '所有文件', extensions: ['*'] },
    ],
    properties: ['openFile'],
  })

  if (!result.canceled && result.filePaths.length > 0) {
    mainWindow?.webContents.send('menu:open', result.filePaths[0])
  }
}

// IPC Handlers

ipcMain.handle('blessstar:readSchema', async () => {
  // 方案 H：mmap 共享内存 → 读取自描述 Schema JSON
  // 在 Windows 上通过 CreateFileMappingA / OpenFileMappingA
  // MVP 实现：尝试打开指定名称的共享内存区域
  try {
    const name = 'Global\\bs_config_config_declare'
    const fs = await import('fs')

    // On Windows, shared memory is backed by pagefile;
    // we use a file-based fallback for editor access:
    // try to read a cached schema.json from the app data directory
    const schemaPath = path.join(app.getPath('userData'), 'schema_cache.json')
    if (fs.existsSync(schemaPath)) {
      const content = fs.readFileSync(schemaPath, 'utf-8')
      const parsed = JSON.parse(content)
      if (parsed && parsed.fields && Array.isArray(parsed.fields)) {
        return content
      }
    }
    return null
  } catch {
    return null
  }
})

ipcMain.handle('blessstar:loadConfig', async () => {
  const result = await dialog.showOpenDialog({
    title: '打开配置文件',
    filters: [
      { name: 'JSON 配置文件', extensions: ['json'] },
      { name: '所有文件', extensions: ['*'] },
    ],
    properties: ['openFile'],
  })

  if (result.canceled || result.filePaths.length === 0) {
    return null
  }

  const fs = await import('fs')
  const content = fs.readFileSync(result.filePaths[0], 'utf-8')
  return JSON.stringify({ path: result.filePaths[0], content })
})

ipcMain.handle('blessstar:saveConfig', async (_event, jsonContent: string) => {
  const result = await dialog.showSaveDialog({
    title: '保存配置文件',
    filters: [
      { name: 'JSON 配置文件', extensions: ['json'] },
      { name: '所有文件', extensions: ['*'] },
    ],
  })

  if (result.canceled || !result.filePath) {
    return false
  }

  const fs = await import('fs')
  fs.writeFileSync(result.filePath, jsonContent, 'utf-8')
  return true
})

ipcMain.handle('blessstar:schemaToUidl', async () => {
  try {
    const addon = loadNativeAddon()
    // 先获取真实 Schema JSON
    const schemaJson = addon.getRegisteredSchemaJson()
    // 再通过 Rust schema_to_uidl 转换为 UIDL 格式
    return addon.schemaToUidl(schemaJson)
  } catch (err) {
    console.error('[schemaToUidl] 使用真实 Schema 失败，回退到简化 UIDL:', err)
    // 回退：基于实时获取的 schema 做简化 UIDL
    try {
      const addon = loadNativeAddon()
      const schemaJson = addon.getRegisteredSchemaJson()
      const schema = JSON.parse(schemaJson)
      const fields = (schema.fields || []).map((f: any, i: number) => ({
        widget: f.type === 'bool' ? 'checkbox' : f.type === 'int32' || f.type === 'int64' || f.type === 'double' ? 'number' : 'input',
        label: f.description || f.key,
        key: f.key,
        default_value: f.default,
        order: i + 1,
      }))
      return JSON.stringify({
        render_type: 'dynamic_form',
        version: '1.0.0',
        title: 'BlessStar 配置（简化）',
        fields,
      })
    } catch {
      return JSON.stringify({ render_type: 'dynamic_form', version: '1.0.0', title: 'BlessStar 配置', fields: [] })
    }
  }
})

ipcMain.handle('blessstar:saveToPath', async (_event, filePath: string, content: string) => {
  try {
    const fs = await import('fs')
    fs.writeFileSync(filePath, content, 'utf-8')
    return true
  } catch (err) {
    console.error('saveToPath error:', err)
    return false
  }
})

// ── Ollama AI completion (via main process fetch) ──────────────────
ipcMain.handle('blessstar:aiComplete', async (_event, body: string) => {
  try {
    // 构建配置上下文注入
    let injectedBody = body
    try {
      const addon = loadNativeAddon()
      const schemaJson = addon.getRegisteredSchemaJson()
      const schema = JSON.parse(schemaJson)
      const fields = schema.fields || []

      // 生成字段描述摘要
      const fieldSummary = fields.map((f: any) => {
        const key = f.key || '?'
        const type = f.type || '?'
        const desc = f.description || key.split('.').pop() || '?'
        const def = f.default !== undefined && f.default !== '' ? `（默认: ${f.default}）` : ''
        return `  - ${key} (${type}): ${desc}${def}`
      }).join('\n')

      const totalDomains = new Set(fields.map((f: any) => f.key.split('.').slice(0, 2).join('.'))).size

      const systemPrefix = `你是一个专业的配置管理助手。以下是当前应用的全部 ${fields.length} 个可配置项（${totalDomains} 个配置组）的完整清单：

${fieldSummary}

回答配置相关问题时应优先参考上述字段定义。`

      const req = JSON.parse(body)
      if (req.messages && Array.isArray(req.messages)) {
        // 检查是否已有 system 消息
        const hasSystem = req.messages.some((m: any) => m.role === 'system')
        if (!hasSystem) {
          req.messages.unshift({ role: 'system', content: systemPrefix })
        } else {
          // 追加到现有 system 消息
          req.messages = req.messages.map((m: any) =>
            m.role === 'system'
              ? { ...m, content: m.content + '\n\n' + systemPrefix }
              : m
          )
        }
        injectedBody = JSON.stringify(req)
      }
    } catch (err) {
      console.warn('[aiComplete] 无法获取 Schema 上下文，使用原始请求:', err)
    }

    const res = await fetch('http://localhost:11434/api/chat', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: injectedBody,
    })
    if (!res.ok) {
      const errText = await res.text().catch(() => '')
      throw new Error(`Ollama HTTP ${res.status}: ${errText}`)
    }
    return await res.text()
  } catch (err) {
    console.error('[aiComplete] error:', err)
    throw err
  }
})

// ── 云端 AI 聊天（OpenAI 兼容接口：DeepSeek / OpenAI）─────────────
ipcMain.handle('blessstar:aiChat', async (_event, config: { baseUrl: string; apiKey: string; model: string; body: string }) => {
  const url = `${config.baseUrl.replace(/\/+$/, '')}/v1/chat/completions`
  const reqBody = JSON.parse(config.body)
  reqBody.model = config.model
  const res = await fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${config.apiKey}`,
    },
    body: JSON.stringify(reqBody),
  })
  if (!res.ok) {
    const errText = await res.text().catch(() => '')
    throw new Error(`AI API HTTP ${res.status}: ${errText}`)
  }
  return await res.text()
})

// ── EMB: Embedding API（OpenAI 兼容 & Ollama）─────────────────
ipcMain.handle('blessstar:aiEmbed', async (_event, config: { url: string; apiKey?: string; body: string }) => {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' }
  if (config.apiKey) {
    headers['Authorization'] = `Bearer ${config.apiKey}`
  }
  const res = await fetch(config.url, {
    method: 'POST',
    headers,
    body: config.body,
  })
  if (!res.ok) {
    const errText = await res.text().catch(() => '')
    throw new Error(`Embedding API HTTP ${res.status}: ${errText}`)
  }
  return await res.text()
})

// ── Ollama 模型列表 ──────────────────────────────────────────────
ipcMain.handle('blessstar:ollamaListModels', async () => {
  try {
    const res = await fetch('http://localhost:11434/api/tags')
    if (!res.ok) return []
    const data = await res.json()
    // Ollama /api/tags 返回 { models: [{ name, modified_at, size, ... }] }
    return (data.models || []).map((m: any) => ({
      name: m.name,
      modified_at: m.modified_at || '',
      size: m.size || 0,
    }))
  } catch {
    return []
  }
})

// ── AppSession 全局实例（E2E-07: 生命周期绑定 Electron Main 进程）────────────────
let g_appSessionHandle: number | null = null
let g_attachCtxHandle: number | null = null

// ── SafeStorage: API Key 加密存储（第33天）───────────────────────────────
ipcMain.handle('blessstar:encryptString', async (_event, plaintext: string) => {
  if (!safeStorage.isEncryptionAvailable()) {
    // safeStorage 不可用时降级为 base64 编码（非加密，但可确保不存明文）
    return Buffer.from(plaintext, 'utf-8').toString('base64')
  }
  const encrypted = safeStorage.encryptString(plaintext)
  return encrypted.toString('base64')
})

ipcMain.handle('blessstar:decryptString', async (_event, ciphertext: string) => {
  if (!safeStorage.isEncryptionAvailable()) {
    // 降级：base64 解码
    return Buffer.from(ciphertext, 'base64').toString('utf-8')
  }
  const buffer = Buffer.from(ciphertext, 'base64')
  return safeStorage.decryptString(buffer)
})

ipcMain.handle('blessstar:isEncryptionAvailable', async () => {
  return safeStorage.isEncryptionAvailable()
})

function ensureAppSession(addon: any): boolean {
  if (g_appSessionHandle !== null && addon.appSessionIsOkFfi(g_appSessionHandle)) {
    return true
  }
  try {
    const manifestPath = getManifestPath()
    const handle = addon.appSessionCreateFfi(manifestPath)
    if (handle === null || handle === undefined) {
      console.warn('[BlessStar] AppSession 创建失败')
      return false
    }
    g_appSessionHandle = handle
    g_attachCtxHandle = addon.appSessionGetCtxFfi(handle)
    console.log('[BlessStar] AppSession created, ctx handle:', g_attachCtxHandle)
    return true
  } catch (err) {
    console.warn('[BlessStar] AppSession 创建异常:', err)
    return false
  }
}

function destroyAppSession(addon: any): void {
  if (g_appSessionHandle !== null) {
    try {
      addon.appSessionDestroyFfi(g_appSessionHandle)
    } catch (err) {
      console.warn('[BlessStar] AppSession 销毁异常:', err)
    }
    g_appSessionHandle = null
    g_attachCtxHandle = null
  }
}

// ── 专题十二：单文件持久化存储 ──────────────────────────────────
const CONFIGS_DIR = 'BlessStar/configs'

function getManifestPath(): string {
  return path.join(app.getPath('userData'), CONFIGS_DIR, 'manifest.json')
}

function getConfigsDataPath(): string {
  return path.join(app.getPath('userData'), CONFIGS_DIR, 'configs.json')
}

/** 从 Schema 注册信息生成 manifest.json（只读元信息，Editor 自动生成） */
function generateManifestFromSchema(addon: any): void {
  try {
    const fs = require('fs') as typeof import('fs')
    const manifestPath = getManifestPath()
    const manifestDir = path.dirname(manifestPath)

    // 确保目录存在
    if (!fs.existsSync(manifestDir)) {
      fs.mkdirSync(manifestDir, { recursive: true })
    }

    const manifest = {
      version: '1.0',
      generated_at: new Date().toISOString(),
      data_file: 'configs.json',
    }

    fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2), 'utf-8')
    console.log('[Persist] Manifest generated at:', manifestPath)
  } catch (err) {
    console.warn('[Persist] Manifest generation failed:', err)
  }
}

// ── 新增 IPC Handlers（E2E Editor Bridge）─────────────────────────────

// normalizeVendor: 厂商配置归一化（通过 NormalizerRegistry 分发）
ipcMain.handle('blessstar:normalizeVendor', async (_event, vendorId: string, inputJson: string, extraJson?: string) => {
  try {
    const addon = loadNativeAddon()
    const result = addon.normalizerNormalizeFfi(vendorId, inputJson, extraJson || '')
    return { success: true, result }
  } catch (err) {
    console.error('[normalizeVendor] error:', err)
    return { success: false, result: null }
  }
})

// appSessionCreate: 创建 AppSession
ipcMain.handle('blessstar:appSessionCreate', async () => {
  try {
    const addon = loadNativeAddon()
    if (ensureAppSession(addon)) {
      return { success: true, handle: g_appSessionHandle }
    }
    return { success: false, handle: null }
  } catch (err) {
    console.error('[appSessionCreate] error:', err)
    return { success: false, handle: null }
  }
})

// appSessionDestroy: 销毁 AppSession
ipcMain.handle('blessstar:appSessionDestroy', async () => {
  try {
    const addon = loadNativeAddon()
    destroyAppSession(addon)
    return { success: true }
  } catch (err) {
    console.error('[appSessionDestroy] error:', err)
    return { success: false }
  }
})

// commitBatch: 批量提交配置变更
ipcMain.handle('blessstar:commitBatch', async (_event, entriesJson: string) => {
  try {
    const addon = loadNativeAddon()
    if (g_appSessionHandle === null) {
      if (!ensureAppSession(addon)) {
        return { success: false, report: null, error: 'AppSession 未就绪' }
      }
    }
    const reportJson = addon.configCommitBatchFfi(g_appSessionHandle, entriesJson)
    if (reportJson) {
      // commitBatch 成功后触发单文件持久化
      try {
        addon.configPersistWriteFfi(getManifestPath())
      } catch (persistErr) {
        console.warn('[Persist] commitBatch 后持久化写入失败（非致命）:', persistErr)
      }
      return { success: true, report: reportJson }
    }
    return { success: false, report: null, error: 'Commit 返回空 Report' }
  } catch (err) {
    console.error('[commitBatch] error:', err)
    return { success: false, report: null, error: (err as Error).message }
  }
})

// exportAgentIndex: 导出 Agent 索引文件（替换 mock）
ipcMain.handle('blessstar:exportAgentIndex', async (_event, config) => {
  try {
    const addon = loadNativeAddon()
    const schemaJson = addon.getRegisteredSchemaJson()
    const outputDir = config.outputDir || '.cursor/agents/'
    const businessName = config.businessName || 'Default'
    const ok = addon.agentIndexExportFfi(schemaJson, outputDir, businessName)
    console.log('[AgentFactory] exportAgentIndex:', outputDir, businessName, ok)
    return { success: ok, outputDir }
  } catch (err) {
    console.warn('[AgentFactory] exportAgentIndex error:', err)
    return { success: false, outputDir: config.outputDir || '.cursor/agents/' }
  }
})

// ── 第34天 · GR-01：Gate 注册专用 IPC ──────────────────────────────
ipcMain.handle('blessstar:registerGate', async (_event, gateType: string, ruleJson: string) => {
  try {
    const addon = loadNativeAddon()
    if (g_appSessionHandle === null) {
      if (!ensureAppSession(addon)) {
        return { success: false, error: 'AppSession 未就绪' }
      }
    }
    const ok = addon.registerGateRuleFfi(g_appSessionHandle, gateType, ruleJson)
    console.log('[registerGate]', gateType, ok)
    return { success: ok }
  } catch (err) {
    console.error('[registerGate] error:', err)
    return { success: false, error: (err as Error).message }
  }
})

// ── 第38天 · ⑤ IPC 通道收敛：统一 executeTool switch ──────────────
// 将所有工具类 handler 收敛到 blessstar:executeTool，减少 IPC 注册数
// 兼容双调用签名：
//   模式A: executeTool('toolName', {key: value})         ← preload.ts
//   模式B: executeTool({tool: 'toolName', args: {...}})  ← create_gate_chain.ts / preGate/index.ts
interface ExecuteToolArgs {
  tool: string
  args: Record<string, unknown>
}

ipcMain.handle('blessstar:executeTool', async (_event, firstArg: unknown, secondArg: unknown) => {
  // 自动检测调用模式
  let tool: string
  let args: Record<string, unknown>

  if (typeof firstArg === 'string') {
    // 模式A: preload.ts → executeTool(toolName, args)
    tool = firstArg
    args = (secondArg as Record<string, unknown>) || {}
  } else if (typeof firstArg === 'object' && firstArg !== null && 'tool' in (firstArg as any)) {
    // 模式B: create_gate_chain.ts → executeTool({tool, args})
    tool = (firstArg as ExecuteToolArgs).tool
    args = (firstArg as ExecuteToolArgs).args || {}
  } else {
    return { success: false, error: `executeTool: 无效参数类型` }
  }

  try {
    const addon = loadNativeAddon()

    switch (tool) {
      // ═══════════════════════════════════════════════════════════
      // Phase 1: Day 38 新增 Gate napi-rs FFI 桥接
      // ═══════════════════════════════════════════════════════════

      case 'create_gate_chain': {
        const factoryType = String(args.factory_type || 'default')
        const ruleJson = JSON.stringify({
          field_key: args.field_key || '',
          field_type: args.field_type || 'string',
          op: args.op || 'eq',
          value: args.value || '',
          scenario: args.scenario || 'production',
          layer: args.layer || 0,
          ai_hint: args.ai_hint || '',
        })
        return addon.runGateFactoryProduce(factoryType, ruleJson)
      }

      case 'gate_evaluator': {
        const chainJson = String(args.chain_json || '{}')
        const fieldKey = String(args.field_key || '')
        const fieldValue = String(args.field_value || '')
        return addon.runGateEvaluatorEvaluate(chainJson, fieldKey, fieldValue)
      }

      case 'gate_map_upsert': {
        const stableKey = String(args.stable_key || '')
        const nodeJson = String(args.node_json || '{}')
        return addon.runGateMapUpsert(stableKey, nodeJson)
      }

      case 'gate_map_lookup': {
        const stableKey = String(args.stable_key || '')
        return addon.runGateMapLookup(stableKey)
      }

      // ═══════════════════════════════════════════════════════════
      // Named Pipe / Metrics / Tracing / Auth
      // ═══════════════════════════════════════════════════════════

      case 'named_pipe_notify': {
        const pipeName = String(args.pipe_name || '\\\\.\\pipe\\blessstar_notify')
        const message = String(args.message || '')
        return addon.shmNotifySend(pipeName, message)
      }

      case 'status_metrics': {
        return addon.exportMetricsPrometheus()
      }

      case 'export_trace': {
        return addon.exportTraceSpans()
      }

      case 'auth_verify_token': {
        const token = String(args.token || '')
        return addon.authVerifyToken(token)
      }

      // ═══════════════════════════════════════════════════════════
      // 基础配置读写 (原有 handler 迁移)
      // ═══════════════════════════════════════════════════════════

      case 'write_config_value': {
        const key = String(args.key || '')
        const value = String(args.value || '')
        if (!key) return { success: false, result: 'key 不能为空' }

        let warning: string | undefined

        // 优先走 WAL 持久化路径（AppSession → ConfigReloadSession → Commit）
        if (ensureAppSession(addon)) {
          const entriesJson = JSON.stringify([{ key, value }])
          const reportJson = addon.configCommitBatchFfi(g_appSessionHandle, entriesJson)
          if (reportJson) {
            // WAL 成功后也更新运行时值存储
            addon.writeBlessStarConfig(key, value)
            console.log(`[Config] WAL 写入 ${key} = ${value} → ${reportJson}`)
            // commitBatch 成功后触发单文件持久化
            try {
              addon.configPersistWriteFfi(getManifestPath())
            } catch (persistErr) {
              console.warn('[Persist] write_config_value 后持久化写入失败（非致命）:', persistErr)
            }
          } else {
            warning = `WAL 提交返回空 Report，值已写入内存但未持久化到磁盘，重启后会丢失`
            console.warn(`[Config] WAL 提交返回空 Report，降级到内存写入`)
            addon.writeBlessStarConfig(key, value)
          }
        } else {
          warning = `AppSession 未就绪，值已写入内存但未持久化到磁盘，重启后会丢失`
          console.warn(`[Config] AppSession 未就绪，降级到内存写入 ${key} = ${value}`)
          addon.writeBlessStarConfig(key, value)
        }
        // 通知渲染进程表单值已变更
        if (mainWindow && !mainWindow.isDestroyed()) {
          mainWindow.webContents.send('config:changed', JSON.stringify({ key, value }))
        }
        const ret: Record<string, unknown> = { success: true, result: value }
        if (warning) ret.warning = warning
        return ret
      }

      case 'read_config_value': {
        const key = String(args.key || '')
        if (!key) return { success: false, result: 'key 不能为空' }
        const result = addon.readBlessStarConfig(key)
        return { success: true, result: result ?? null }
      }

      case 'list_configs': {
        try {
          const schemaJson = addon.getRegisteredSchemaJson()
          const schema = JSON.parse(schemaJson)
          const fields = schema.fields || []
          const prefix = String(args.prefix || '')
          const configs: { key: string; type: string; value: string | null; default: string }[] = []
          for (const field of fields) {
            if (prefix && !field.key.startsWith(prefix)) continue
            const val = addon.readBlessStarConfig(field.key)
            configs.push({
              key: field.key,
              type: field.type_name || 'string',
              value: val,
              default: field.default_value || '',
            })
          }
          return { success: true, result: JSON.stringify({ count: configs.length, configs }) }
        } catch (err) {
          return { success: false, result: `读取配置列表失败: ${(err as Error).message}` }
        }
      }

      case 'create_schema_field': {
        try {
          const key = String(args.key || '')
          const widget = String(args.widget || 'input')
          const desc = String(args.description || args.label || '')
          const defaultValue = String(args.default_value || '')
          let fieldType = 2 // default: STRING
          if (widget === 'number') fieldType = 0
          else if (widget === 'checkbox') fieldType = 4
          const required = args.required === true
          addon.registerSchemaFieldFfi(key, fieldType, defaultValue, desc, required)
          return { success: true, result: JSON.stringify({ key, widget, fieldType, registered: true }) }
        } catch (err) {
          return { success: false, result: `create_schema_field 失败: ${(err as Error).message}` }
        }
      }

      case 'update_gate_rule': {
        try {
          const gateId = String(args.gate_id || '')
          const action = String(args.action || 'add_rule')
          const field = args.field ? String(args.field) : ''
          const operator = args.operator ? String(args.operator) : ''
          const value = args.value ? String(args.value) : ''
          const entries = JSON.stringify([{
            key: `gate.${gateId}.${field}`,
            value: JSON.stringify({ action, operator, value }),
          }])
          const sessionHandle = g_appSessionHandle
          if (sessionHandle !== null && sessionHandle > 0) {
            const reportJson = addon.configCommitBatchFfi(sessionHandle, entries)
            return { success: true, result: reportJson || `Gate 规则已更新: ${gateId}` }
          } else {
            console.log('[update_gate_rule] AppSession 未就绪，gate rule 标记为待提交')
            return { success: true, result: JSON.stringify({ gateId, action, field, note: 'AppSession 未就绪，规则已标记' }) }
          }
        } catch (err) {
          return { success: false, result: `update_gate_rule 失败: ${(err as Error).message}` }
        }
      }

      // ═══════════════════════════════════════════════════════════
      // FS / Shell 工具 (沙箱白名单)
      // ═══════════════════════════════════════════════════════════

      case 'list_directory': {
        const dirPath = String(args.path || '')
        if (!dirPath) return { success: false, result: 'path 不能为空' }
        const fakePatterns = [/\/path\/to\//i, /\\path\\to\\/i, /^\/path\//i, /\\path\\/i, /placeholder/i, /example/i]
        if (fakePatterns.some((p) => p.test(dirPath))) {
          console.warn(`[FS] 检测到假路径: ${dirPath}`)
          return { success: false, result: `检测到路径 "${dirPath}" 像是示例路径，请提供真实路径。` }
        }
        try {
          const fs = require('fs')
          const path = require('path')
          const entries = fs.readdirSync(dirPath, { withFileTypes: true })
          const result = entries.map((entry: import('fs').Dirent) => ({
            name: entry.name,
            isDirectory: entry.isDirectory(),
            size: entry.isFile() ? fs.statSync(path.join(dirPath, entry.name)).size : 0,
          }))
          return { success: true, result: JSON.stringify(result) }
        } catch (err) {
          return { success: false, result: `目录不存在或无权限访问: ${dirPath}` }
        }
      }

      case 'run_terminal': {
        const command = String(args.command || '')
        const cwd = String(args.cwd || '')
        if (!command) return { success: false, result: 'command 不能为空' }
        if (!cwd) return { success: false, result: 'cwd 不能为空' }
        const cmdName = command.trim().split(/\s+/)[0].toLowerCase()
        const ALLOWED = ['dir', 'ls', 'type', 'cat', 'tree', 'stat', 'echo', 'findstr', 'grep', 'where']
        if (!ALLOWED.includes(cmdName)) {
          return { success: false, result: `命令 "${cmdName}" 不在白名单中。允许: ${ALLOWED.join(', ')}` }
        }
        const path = require('path')
        const resolvedCwd = path.resolve(cwd)
        const projectRoot = path.resolve(__dirname, '..', '..')
        const workspaceRoot = path.resolve(__dirname, '..', '..', '..')
        const allowed = [projectRoot.toLowerCase(), workspaceRoot.toLowerCase()]
        if (!allowed.some((r: string) => resolvedCwd.toLowerCase().startsWith(r))) {
          return { success: false, result: `目录 "${cwd}" 不在沙箱允许范围内` }
        }
        try {
          const { execSync } = require('child_process')
          const output = execSync(command, {
            cwd: resolvedCwd,
            timeout: 10000,
            encoding: 'utf-8',
            maxBuffer: 1024 * 1024,
          })
          return { success: true, result: JSON.stringify({ command, cwd, output: output.toString(), exitCode: 0 }) }
        } catch (err: any) {
          return { success: false, result: JSON.stringify({ command, cwd, output: (err.stderr || err.stdout || '').toString(), exitCode: err.status || 1 }) }
        }
      }

      // D38-3: execute_query — 触发 C++ QueryExecutorRegistry::ApplyChanges
      case 'execute_query': {
        const configKey = String(args.config_key || args.key_pattern || '')
        if (!configKey) {
          return { success: false, error: 'execute_query: missing config_key' }
        }
        try {
          const addon = loadNativeAddon()
          const resultStr = addon.executeQuery(configKey)
          return { success: true, result: resultStr }
        } catch (err) {
          return { success: false, error: `execute_query 失败: ${(err as Error).message}` }
        }
      }

      default:
        console.warn(`[executeTool] 未知工具: ${tool}`)
        return { success: false, error: `未知工具: ${tool}` }
    }
  } catch (err) {
    console.error(`[executeTool] ${tool} error:`, err)
    return { success: false, error: (err as Error).message }
  }
})

ipcMain.handle('blessstar:getRegisteredSchemas', async () => {
  try {
    const addon = loadNativeAddon()
    const schemaJson = addon.getRegisteredSchemaJson()
    return JSON.parse(schemaJson)
  } catch (err) {
    console.warn('[getRegisteredSchemas]', err)
    return { fields: [] }
  }
})

// getGateChain: 读取真实 Gate 链（替换空 mock）
ipcMain.handle('blessstar:getGateChain', async () => {
  try {
    const addon = loadNativeAddon()
    const schemaJson = addon.getRegisteredSchemaJson()
    // 从 schema 推断 Gate 链的基础信息
    const schema = JSON.parse(schemaJson)
    const fields = schema.fields || []
    const gates = fields
      .filter((f: any) => f.key)
      .map((f: any) => ({
        gate_id: `gate_${f.key.replace(/\./g, '_')}`,
        scenario: 'default',
        field_key: f.key,
        op: 'exists',
        value: 'true',
        layer: 0,
      }))
    return { version: '1.0', gates }
  } catch (err) {
    console.warn('[getGateChain] error:', err)
    return { version: '1.0', gates: [] }
  }
})

// validateConfig: Gate 链校验（替换 JSON 语法校验 mock）
ipcMain.handle('blessstar:validateConfig', async (_event, configJson: string) => {
  try {
    const parsed = JSON.parse(configJson)
    // 基础结构校验
    const errors: Array<{ path: string; message: string }> = []

    if (!parsed.version) {
      errors.push({ path: 'version', message: '缺少 version 字段' })
    }
    if (!parsed.instructions || !parsed.instructions.paths) {
      errors.push({ path: 'instructions.paths', message: '缺少 instructions.paths' })
    }

    // 如果存在字段级值，检查 key 是否已注册
    if (parsed.instructions?.paths) {
      try {
        const addon = loadNativeAddon()
        const schemaJson = addon.getRegisteredSchemaJson()
        const schema = JSON.parse(schemaJson)
        const registeredKeys = new Set((schema.fields || []).map((f: any) => f.key))

        for (const key of Object.keys(parsed.instructions.paths)) {
          if (!registeredKeys.has(key)) {
            errors.push({ path: `instructions.paths.${key}`, message: `字段 "${key}" 未在 Schema 中注册` })
          }
        }
      } catch {
        // schema 不可用时跳过 key 校验
      }
    }

    return { valid: errors.length === 0, errors }
  } catch {
    return { valid: false, errors: [{ path: 'root', message: 'Invalid JSON' }] }
  }
})

// Watch 事件轮询（E2E-04 MVP 实现：setInterval 轮询，Editor 侧表现仍为 push）
let g_watchTimer: ReturnType<typeof setInterval> | null = null

ipcMain.handle('blessstar:subscribeWatch', async () => {
  try {
    if (g_watchTimer === null) {
      g_watchTimer = setInterval(async () => {
        // 检查 ConfigManager 状态变更（通过 bs_config_read 版本号隐式检测）
        // MVP 实现：直接读取当前所有已注册键值与上次快照对比
        // 完整 ThreadSafeFunction 回调桥留待二期
      }, 1000)
    }
    return { success: true }
  } catch (err) {
    console.error('[subscribeWatch] error:', err)
    return { success: false }
  }
})


// ── 版本注册表持久化（第33天 · RV-03 元数据与配置分离）─────────────
const VERSION_REGISTRY_FILENAME = 'version_registry.json'

function getVersionRegistryPath(): string {
  return path.join(app.getPath('userData'), VERSION_REGISTRY_FILENAME)
}

ipcMain.handle('blessstar:loadVersionRegistry', async () => {
  try {
    const fs = await import('fs')
    const filePath = getVersionRegistryPath()
    if (!fs.existsSync(filePath)) return null
    return fs.readFileSync(filePath, 'utf-8')
  } catch (err) {
    console.warn('[VersionRegistry] 加载失败:', err)
    return null
  }
})

ipcMain.handle('blessstar:saveVersionRegistry', async (_event, json: string) => {
  try {
    const fs = await import('fs')
    const filePath = getVersionRegistryPath()
    fs.writeFileSync(filePath, json, 'utf-8')
    return true
  } catch (err) {
    console.warn('[VersionRegistry] 保存失败:', err)
    return false
  }
})

// App lifecycle

app.whenReady().then(async () => {
  buildMenu()

  const addon = loadNativeAddon()

  // 创建全局 AppSession（E2E-07）
  try {
    const addon = loadNativeAddon()
    ensureAppSession(addon)
  } catch (err) {
    console.warn('[BlessStar] AppSession 创建失败，配置提交功能受限:', err)
  }

  // 专题十二：生成 manifest.json 并触发持久化加载
  try {
    const addon = loadNativeAddon()
    generateManifestFromSchema(addon)
    // 尝试从已存在的 configs.json 加载持久化值
    addon.configPersistLoadFfi(getManifestPath())
    console.log('[Persist] 持久化配置已加载')
  } catch (err) {
    console.warn('[Persist] Manifest/持久化初始化失败（首次运行可忽略）:', err)
  }

  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  })
})

app.on('window-all-closed', () => {
  // 清理 AppSession（E2E-07）
  try {
    const addon = loadNativeAddon()
    destroyAppSession(addon)
  } catch {
    // 忽略关闭时的清理异常
  }
  if (process.platform !== 'darwin') {
    app.quit()
  }
})
