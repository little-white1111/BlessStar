import {
  app,
  BrowserWindow,
  Menu,
  ipcMain,
  dialog,
  MenuItemConstructorOptions,
} from 'electron'
import path from 'path'

const isDev = !app.isPackaged

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
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'))
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
    // TODO: 接入 napi-rs addon
    // 返回模拟 UIDL JSON for demo
    const mockUidl = {
      render_type: 'dynamic_form',
      version: '1.0.0',
      title: '数据库连接配置',
      description: '配置数据库连接参数',
      fields: [
        {
          widget: 'input',
          label: '主机地址',
          key: 'host',
          required: true,
          placeholder: 'localhost',
          default_value: 'localhost',
          validation: { max_length: 255 },
          order: 1,
        },
        {
          widget: 'number',
          label: '端口号',
          key: 'port',
          required: true,
          default_value: 3306,
          validation: { min: 1, max: 65535 },
          order: 2,
        },
        {
          widget: 'input',
          label: '数据库名',
          key: 'database',
          required: true,
          placeholder: 'mydb',
          validation: { max_length: 128 },
          order: 3,
        },
        {
          widget: 'input',
          label: '用户名',
          key: 'username',
          required: true,
          placeholder: 'root',
          order: 4,
        },
        {
          widget: 'input',
          label: '密码',
          key: 'password',
          required: true,
          placeholder: '••••••••',
          order: 5,
        },
        {
          widget: 'group',
          label: '连接池设置',
          key: 'pool',
          order: 6,
          children: [
            {
              widget: 'number',
              label: '最小连接数',
              key: 'min_size',
              default_value: 2,
              validation: { min: 1, max: 100 },
              order: 1,
            },
            {
              widget: 'number',
              label: '最大连接数',
              key: 'max_size',
              default_value: 10,
              validation: { min: 1, max: 200 },
              order: 2,
            },
            {
              widget: 'number',
              label: '超时时间(秒)',
              key: 'timeout',
              default_value: 30,
              validation: { min: 1, max: 3600 },
              order: 3,
            },
          ],
        },
        {
          widget: 'select',
          label: '字符集',
          key: 'charset',
          default_value: 'utf8mb4',
          options: [
            { label: 'UTF-8', value: 'utf8' },
            { label: 'UTF-8 MB4', value: 'utf8mb4' },
            { label: 'Latin1', value: 'latin1' },
            { label: 'GBK', value: 'gbk' },
          ],
          order: 7,
        },
        {
          widget: 'checkbox',
          label: '启用 SSL',
          key: 'ssl_enabled',
          default_value: false,
          order: 8,
        },
        {
          widget: 'repeatable',
          label: '额外参数',
          key: 'extra_params',
          order: 9,
          children: [
            {
              widget: 'input',
              label: '参数名',
              key: 'name',
              required: true,
              order: 1,
            },
            {
              widget: 'input',
              label: '参数值',
              key: 'value',
              required: true,
              order: 2,
            },
          ],
        },
      ],
    }
    return JSON.stringify(mockUidl)
  } catch (err) {
    console.error('schemaToUidl error:', err)
    throw err
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
  const res = await fetch('http://localhost:11434/api/chat', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body,
  })
  if (!res.ok) {
    const errText = await res.text().catch(() => '')
    throw new Error(`Ollama HTTP ${res.status}: ${errText}`)
  }
  return await res.text()
})

// ── Agent Factory IPC handlers (OPT-04) ─────────────────────────────
ipcMain.handle('blessstar:exportAgentIndex', async (_event, config) => {
  console.log('[AgentFactory] exportAgentIndex:', config)
  return { success: true, outputDir: config.outputDir || '.cursor/agents/' }
})

ipcMain.handle('blessstar:getRegisteredSchemas', async () => {
  return []
})

ipcMain.handle('blessstar:getGateChain', async () => {
  return { version: '1.0', gates: [] }
})

ipcMain.handle('blessstar:validateConfig', async (_event, configJson: string) => {
  try {
    JSON.parse(configJson)
    return { valid: true, errors: [] }
  } catch {
    return { valid: false, errors: [{ path: 'root', message: 'Invalid JSON' }] }
  }
})

ipcMain.handle('blessstar:executeTool', async (_event, toolName: string, args: unknown) => {
  console.log('[AgentFactory] executeTool:', toolName, args)
  return { success: true, result: `Tool ${toolName} executed (mock)` }
})

// App lifecycle

app.whenReady().then(() => {
  buildMenu()
  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  })
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
  }
})
