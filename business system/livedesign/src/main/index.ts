/**
 * 主进程入口
 *
 * 架构不变量：
 *   #1  — IPC Router 是唯一消息路由 — 全局 ipcRouter 单例
 *   #4  — LLM Service 崩溃不影响 UI — ProcessManager 自动重启
 *   #5  — 所有用户数据仅存储在本地 — electron-store + better-sqlite3
 *   #10 — 悬浮球位置变更实时持久化 — WindowManager.persistWindowBounds
 *
 * 启动流程：
 *   1. app.whenReady()
 *   2. 初始化配置存储 (configStore)
 *   3. 初始化 IPC Router (ipcRouter)
 *   4. 创建主窗口 (windowManager)
 *   5. 初始化原生桥接 (nativeBridge)
 *   6. 启动系统托盘
 *   7. 启动子进程 (LLM Service, Plugin Host)
 *   8. 加载当前角色
 */

import { app, BrowserWindow, ipcMain, Menu } from 'electron';
import { IpcEnvelope, IpcChannel, ProcessId } from '../shared/ipc-protocol';
import { ConfigValue } from '../shared/config-schema';
import { ipcRouter } from './ipc-router';
import { windowManager } from './window-manager';
import { nativeBridge } from './native-bridge';
import { processManager } from './process-manager';
import { configStore } from './storage/config-store';
import { dialogStore } from './storage/dialog-store';
import { characterStore } from './storage/character-store';
import { provideBlessStarAdapters, Adapters } from '../provider';
import { CachedReader } from '../provider/cached-reader';
import { StoreConfigReader } from '../adapters/store-config-reader';

/**
 * 全局配置适配器实例（架构不变量 #3, #6）
 * 暴露给主进程中需要读取配置的模块使用。
 * 无需 electron-store 调用，统一通过 Port 接口访问。
 */
export let adapters: Adapters | undefined;

// ==================== 应用生命周期管理 ====================

/**
 * 应用启动时的初始化流程
 */
async function initialize(): Promise<void> {
  console.log('[Main] 主进程启动...');

  // 1. 加载配置存储（架构不变量 #5）
  console.log('[Main] 配置存储已就绪');
  const allConfig = configStore.getAll();
  console.log(`[Main] 已加载 ${Object.keys(allConfig).length} 项配置`);

  // 1b. 初始化 Port-Adapter 层（架构不变量 #3, #6）
  //     StoreConfigReader 桥接 ConfigStore → ConfigReader
  //     CachedReader 提供秒级准实时缓存（30s 刷新周期）
  //     provideBlessStarAdapters 一行实例化所有域适配器
  const storeReader = new StoreConfigReader(configStore);
  const cachedReader = new CachedReader(storeReader, 30_000);
  adapters = provideBlessStarAdapters(cachedReader);
  console.log('[Main] Port-Adapter 层已初始化');

  // 2. 注册 IPC 通道（架构不变量 #1）
  setupIpcChannels();
  console.log('[Main] IPC 通道已注册');

  // 3. 创建主窗口
  const mainWindow = windowManager.createMainWindow();
  console.log('[Main] 主窗口已创建');

  // 4. 初始化原生桥接
  nativeBridge.init(mainWindow);

  // 5. 启动系统托盘
  const iconPath = getTrayIconPath();
  nativeBridge.createTray(iconPath);
  console.log('[Main] 系统托盘已启动');

  // 6. 初始化 IPC Router 的 Renderer 消息监听
  ipcMain.on('ipc-message', (event, envelope: IpcEnvelope) => {
    ipcRouter.handleRendererMessage(event, envelope);
  });

  // 7. 启动子进程（架构不变量 #4）
  await startChildProcesses();

  // 8. 加载角色卡存储和创建默认角色
  characterStore.createDefaultCharacterIfEmpty();
  const characters = characterStore.listCharacters();
  console.log(`[Main] 已加载 ${characters.length} 个角色卡`);

  // 9. 加载当前角色
  const currentCharacter = characterStore.getCurrentCharacter();
  if (currentCharacter) {
    console.log(`[Main] 当前角色: ${currentCharacter.name}`);
  }

  // 10. 设置应用退出处理
  app.on('before-quit', () => {
    handleBeforeQuit();
  });

  console.log('[Main] 主进程初始化完成');
}

/**
 * 获取托盘图标路径
 */
function getTrayIconPath(): string {
  // 尝试多个可能的图标路径
  const possiblePaths = [
    pathJoin(__dirname, '..', '..', 'resources', 'tray-icon.png'),
    pathJoin(__dirname, '..', '..', 'resources', 'icon.png'),
    pathJoin(__dirname, '..', '..', 'resources', 'tray-icon.ico'),
  ];

  for (const p of possiblePaths) {
    try {
      if (require('fs').existsSync(p)) {
        return p;
      }
    } catch {
      continue;
    }
  }

  // 返回默认路径，native-bridge 会处理不存在的图标
  return possiblePaths[0];
}

/**
 * 路径拼接辅助（避免在 Electron 主进程中使用 path.join 导致模块加载冲突）
 */
function pathJoin(...segments: string[]): string {
  const path = require('path');
  return path.join(...segments);
}

/**
 * 启动子进程
 * 架构不变量 #4：子进程崩溃不影响主进程 UI，ProcessManager 自动重启
 */
async function startChildProcesses(): Promise<void> {
  try {
    console.log('[Main] 启动 LLM Service 子进程...');
    await processManager.startLlmService();
    console.log('[Main] LLM Service 子进程已启动');
  } catch (err) {
    console.error('[Main] LLM Service 启动失败（不影响主进程继续启动）:', err);
    // 架构不变量 #4：子进程启动失败不影响主进程
  }

  try {
    console.log('[Main] 启动 Plugin Host 子进程...');
    await processManager.startPluginHost();
    console.log('[Main] Plugin Host 子进程已启动');
  } catch (err) {
    console.error('[Main] Plugin Host 启动失败（不影响主进程继续启动）:', err);
    // 架构不变量 #4：子进程启动失败不影响主进程
  }
}

/**
 * 注册 IPC 处理通道
 *
 * 架构不变量 #1：所有 Renderer → 子进程 的消息经 IPC Router 路由，
 * 这里只注册需要主进程直接处理的消息
 */
function setupIpcChannels(): void {
  // ==================== 窗口操作 ====================

  ipcMain.handle('window:minimize', () => {
    windowManager.minimize();
  });

  ipcMain.handle('window:maximize', () => {
    windowManager.toggleMaximize();
  });

  ipcMain.handle('window:close', () => {
    windowManager.close();
  });

  ipcMain.handle('window:isMaximized', () => {
    return windowManager.getMainWindow()?.isMaximized() ?? false;
  });

  ipcMain.handle('window:setAlwaysOnTop', (_event, alwaysOnTop: boolean) => {
    windowManager.setAlwaysOnTop(alwaysOnTop);
  });

  // ==================== 浮球位置（架构不变量 #10） ====================

  ipcMain.handle('floatball:savePosition', (_event, x: number, y: number) => {
    windowManager.saveFloatBallPosition(x, y);
  });

  ipcMain.handle('floatball:getPosition', () => {
    return windowManager.getFloatBallPosition();
  });

  ipcMain.handle('floatball:setEnabled', (_event, enabled: boolean) => {
    windowManager.setFloatBallEnabled(enabled);
  });

  ipcMain.handle('floatball:getEnabled', () => {
    return windowManager.getFloatBallEnabled();
  });

  // ==================== 原生对话框 ====================

  ipcMain.handle('dialog:openFile', async (_event, options) => {
    return nativeBridge.showOpenDialog(options);
  });

  ipcMain.handle('dialog:saveFile', async (_event, options) => {
    return nativeBridge.showSaveDialog(options);
  });

  // ==================== 系统通知 ====================

  ipcMain.handle('notification:show', (_event, title: string, body: string) => {
    nativeBridge.showNotification(title, body);
  });

  // ==================== 配置操作（架构不变量 #5） ====================

  ipcMain.handle('config:get', (_event, key: string) => {
    return configStore.get(key);
  });

  ipcMain.handle('config:set', (_event, key: string, value: unknown) => {
    configStore.set(key, value as ConfigValue);
    // 广播配置变更（架构不变量 #1）
    ipcRouter.send(ProcessId.RENDERER, IpcChannel.CONFIG_CHANGED, { key, value })
      .catch((err) => console.warn('[Main] 广播配置变更失败:', err));
  });

  ipcMain.handle('config:getAll', () => {
    return configStore.getAll();
  });

  ipcMain.handle('config:reset', (_event, key: string) => {
    configStore.reset(key);
  });

  // ==================== 角色卡操作 ====================

  ipcMain.handle('character:list', () => {
    return characterStore.listCharacters();
  });

  ipcMain.handle('character:get', (_event, characterId: string) => {
    return characterStore.getCharacter(characterId);
  });

  ipcMain.handle('character:getCurrent', () => {
    return characterStore.getCurrentCharacter();
  });

  ipcMain.handle('character:switch', async (_event, characterId: string) => {
    // 切换角色（架构不变量 #7、#12）
    const card = characterStore.switchCharacter(characterId);
    if (!card) {
      return { success: false, error: '角色卡不存在或校验失败' };
    }

    // 架构不变量 #12：角色切换时清空上下文
    dialogStore.clearCharacterContext(characterId);

    // 通知 LLM Service 切换角色（架构不变量 #1）
    try {
      await ipcRouter.send(
        ProcessId.LLM_SERVICE,
        IpcChannel.LLM_SWITCH_CHARACTER,
        { characterId, characterCard: card },
      );
    } catch (err) {
      console.warn('[Main] 通知 LLM Service 切换角色失败:', err);
    }

    // 广播角色切换
    ipcRouter.send(ProcessId.RENDERER, IpcChannel.CONFIG_CHANGED, {
      type: 'character:switched',
      characterId,
      characterName: card.name,
    }).catch((err) => console.warn('[Main] 广播角色切换失败:', err));

    return { success: true, character: card };
  });

  // ==================== 对话历史操作 ====================

  ipcMain.handle('dialog:createSession', (_event, characterId: string) => {
    return dialogStore.createSession(characterId);
  });

  ipcMain.handle('dialog:getMessages', (_event, sessionId: string, options) => {
    return dialogStore.getSessionMessages(sessionId, options);
  });

  ipcMain.handle('dialog:getSessions', (_event, limit?: number) => {
    return dialogStore.getRecentSessions(limit);
  });

  ipcMain.handle('dialog:deleteSession', (_event, sessionId: string) => {
    dialogStore.deleteSession(sessionId);
  });

  // ==================== 进程管理 ====================

  ipcMain.handle('process:getStatus', (_event, processId: ProcessId) => {
    return processManager.getProcessStatus(processId);
  });

  ipcMain.handle('process:getAllStatuses', () => {
    return processManager.getAllStatuses();
  });

  ipcMain.handle('process:restart', async (_event, processId: ProcessId) => {
    await processManager.restartProcess(processId);
  });

  // ==================== 应用信息 ====================

  ipcMain.handle('app:getVersion', () => {
    return app.getVersion();
  });

  ipcMain.handle('app:getPlatform', () => {
    return process.platform;
  });

  ipcMain.handle('app:getConfigDir', () => {
    return app.getPath('userData');
  });

  ipcMain.handle('app:getCharactersDir', () => {
    return characterStore.getCharactersDir();
  });
}

/**
 * 应用退出前清理
 */
function handleBeforeQuit(): void {
  console.log('[Main] 应用退出，清理资源...');

  // 停止所有子进程
  processManager.stopAll();

  // 销毁 IPC Router
  ipcRouter.dispose();

  // 销毁窗口管理器
  windowManager.setQuitting(true);
  windowManager.dispose();

  // 销毁托盘
  nativeBridge.destroyTray();

  // 关闭数据库连接
  dialogStore.close();

  console.log('[Main] 资源清理完成');
}

// ==================== Electron 应用事件 ====================

app.whenReady().then(async () => {
  try {
    await initialize();
  } catch (err) {
    console.error('[Main] 初始化失败:', err);
    // 初始化失败仍然让应用继续运行，不影响已初始化的功能
  }
});

app.on('window-all-closed', () => {
  // macOS 上通常不退出应用（Cmd+Q 才退出）
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  // macOS：点击 Dock 图标时重新创建窗口
  if (windowManager.getMainWindow() === null) {
    windowManager.createMainWindow();
  } else {
    windowManager.show();
  }
});

// 处理未捕获的异常
process.on('uncaughtException', (err) => {
  console.error('[Main] 未捕获的异常:', err);
  // 不退出进程，确保 UI 继续运行（架构不变量 #4 的精神）
});

process.on('unhandledRejection', (reason) => {
  console.warn('[Main] 未处理的 Promise 拒绝:', reason);
});
