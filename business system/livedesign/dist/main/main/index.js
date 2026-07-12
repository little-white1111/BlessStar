"use strict";
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
Object.defineProperty(exports, "__esModule", { value: true });
const electron_1 = require("electron");
const ipc_protocol_1 = require("../shared/ipc-protocol");
const ipc_router_1 = require("./ipc-router");
const window_manager_1 = require("./window-manager");
const native_bridge_1 = require("./native-bridge");
const process_manager_1 = require("./process-manager");
const config_store_1 = require("./storage/config-store");
const dialog_store_1 = require("./storage/dialog-store");
const character_store_1 = require("./storage/character-store");
// ==================== 应用生命周期管理 ====================
/**
 * 应用启动时的初始化流程
 */
async function initialize() {
    console.log('[Main] 主进程启动...');
    // 1. 加载配置存储（架构不变量 #5）
    console.log('[Main] 配置存储已就绪');
    const allConfig = config_store_1.configStore.getAll();
    console.log(`[Main] 已加载 ${Object.keys(allConfig).length} 项配置`);
    // 2. 注册 IPC 通道（架构不变量 #1）
    setupIpcChannels();
    console.log('[Main] IPC 通道已注册');
    // 3. 创建主窗口
    const mainWindow = window_manager_1.windowManager.createMainWindow();
    console.log('[Main] 主窗口已创建');
    // 4. 初始化原生桥接
    native_bridge_1.nativeBridge.init(mainWindow);
    // 5. 启动系统托盘
    const iconPath = getTrayIconPath();
    native_bridge_1.nativeBridge.createTray(iconPath);
    console.log('[Main] 系统托盘已启动');
    // 6. 初始化 IPC Router 的 Renderer 消息监听
    electron_1.ipcMain.on('ipc-message', (event, envelope) => {
        ipc_router_1.ipcRouter.handleRendererMessage(event, envelope);
    });
    // 7. 启动子进程（架构不变量 #4）
    await startChildProcesses();
    // 8. 加载角色卡存储和创建默认角色
    character_store_1.characterStore.createDefaultCharacterIfEmpty();
    const characters = character_store_1.characterStore.listCharacters();
    console.log(`[Main] 已加载 ${characters.length} 个角色卡`);
    // 9. 加载当前角色
    const currentCharacter = character_store_1.characterStore.getCurrentCharacter();
    if (currentCharacter) {
        console.log(`[Main] 当前角色: ${currentCharacter.name}`);
    }
    // 10. 设置应用退出处理
    electron_1.app.on('before-quit', () => {
        handleBeforeQuit();
    });
    console.log('[Main] 主进程初始化完成');
}
/**
 * 获取托盘图标路径
 */
function getTrayIconPath() {
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
        }
        catch {
            continue;
        }
    }
    // 返回默认路径，native-bridge 会处理不存在的图标
    return possiblePaths[0];
}
/**
 * 路径拼接辅助（避免在 Electron 主进程中使用 path.join 导致模块加载冲突）
 */
function pathJoin(...segments) {
    const path = require('path');
    return path.join(...segments);
}
/**
 * 启动子进程
 * 架构不变量 #4：子进程崩溃不影响主进程 UI，ProcessManager 自动重启
 */
async function startChildProcesses() {
    try {
        console.log('[Main] 启动 LLM Service 子进程...');
        await process_manager_1.processManager.startLlmService();
        console.log('[Main] LLM Service 子进程已启动');
    }
    catch (err) {
        console.error('[Main] LLM Service 启动失败（不影响主进程继续启动）:', err);
        // 架构不变量 #4：子进程启动失败不影响主进程
    }
    try {
        console.log('[Main] 启动 Plugin Host 子进程...');
        await process_manager_1.processManager.startPluginHost();
        console.log('[Main] Plugin Host 子进程已启动');
    }
    catch (err) {
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
function setupIpcChannels() {
    // ==================== 窗口操作 ====================
    electron_1.ipcMain.handle('window:minimize', () => {
        window_manager_1.windowManager.minimize();
    });
    electron_1.ipcMain.handle('window:maximize', () => {
        window_manager_1.windowManager.toggleMaximize();
    });
    electron_1.ipcMain.handle('window:close', () => {
        window_manager_1.windowManager.close();
    });
    electron_1.ipcMain.handle('window:isMaximized', () => {
        return window_manager_1.windowManager.getMainWindow()?.isMaximized() ?? false;
    });
    electron_1.ipcMain.handle('window:setAlwaysOnTop', (_event, alwaysOnTop) => {
        window_manager_1.windowManager.setAlwaysOnTop(alwaysOnTop);
    });
    // ==================== 浮球位置（架构不变量 #10） ====================
    electron_1.ipcMain.handle('floatball:savePosition', (_event, x, y) => {
        window_manager_1.windowManager.saveFloatBallPosition(x, y);
    });
    electron_1.ipcMain.handle('floatball:getPosition', () => {
        return window_manager_1.windowManager.getFloatBallPosition();
    });
    electron_1.ipcMain.handle('floatball:setEnabled', (_event, enabled) => {
        window_manager_1.windowManager.setFloatBallEnabled(enabled);
    });
    electron_1.ipcMain.handle('floatball:getEnabled', () => {
        return window_manager_1.windowManager.getFloatBallEnabled();
    });
    // ==================== 原生对话框 ====================
    electron_1.ipcMain.handle('dialog:openFile', async (_event, options) => {
        return native_bridge_1.nativeBridge.showOpenDialog(options);
    });
    electron_1.ipcMain.handle('dialog:saveFile', async (_event, options) => {
        return native_bridge_1.nativeBridge.showSaveDialog(options);
    });
    // ==================== 系统通知 ====================
    electron_1.ipcMain.handle('notification:show', (_event, title, body) => {
        native_bridge_1.nativeBridge.showNotification(title, body);
    });
    // ==================== 配置操作（架构不变量 #5） ====================
    electron_1.ipcMain.handle('config:get', (_event, key) => {
        return config_store_1.configStore.get(key);
    });
    electron_1.ipcMain.handle('config:set', (_event, key, value) => {
        config_store_1.configStore.set(key, value);
        // 广播配置变更（架构不变量 #1）
        ipc_router_1.ipcRouter.send(ipc_protocol_1.ProcessId.RENDERER, ipc_protocol_1.IpcChannel.CONFIG_CHANGED, { key, value })
            .catch((err) => console.warn('[Main] 广播配置变更失败:', err));
    });
    electron_1.ipcMain.handle('config:getAll', () => {
        return config_store_1.configStore.getAll();
    });
    electron_1.ipcMain.handle('config:reset', (_event, key) => {
        config_store_1.configStore.reset(key);
    });
    // ==================== 角色卡操作 ====================
    electron_1.ipcMain.handle('character:list', () => {
        return character_store_1.characterStore.listCharacters();
    });
    electron_1.ipcMain.handle('character:get', (_event, characterId) => {
        return character_store_1.characterStore.getCharacter(characterId);
    });
    electron_1.ipcMain.handle('character:getCurrent', () => {
        return character_store_1.characterStore.getCurrentCharacter();
    });
    electron_1.ipcMain.handle('character:switch', async (_event, characterId) => {
        // 切换角色（架构不变量 #7、#12）
        const card = character_store_1.characterStore.switchCharacter(characterId);
        if (!card) {
            return { success: false, error: '角色卡不存在或校验失败' };
        }
        // 架构不变量 #12：角色切换时清空上下文
        dialog_store_1.dialogStore.clearCharacterContext(characterId);
        // 通知 LLM Service 切换角色（架构不变量 #1）
        try {
            await ipc_router_1.ipcRouter.send(ipc_protocol_1.ProcessId.LLM_SERVICE, ipc_protocol_1.IpcChannel.LLM_SWITCH_CHARACTER, { characterId, characterCard: card });
        }
        catch (err) {
            console.warn('[Main] 通知 LLM Service 切换角色失败:', err);
        }
        // 广播角色切换
        ipc_router_1.ipcRouter.send(ipc_protocol_1.ProcessId.RENDERER, ipc_protocol_1.IpcChannel.CONFIG_CHANGED, {
            type: 'character:switched',
            characterId,
            characterName: card.name,
        }).catch((err) => console.warn('[Main] 广播角色切换失败:', err));
        return { success: true, character: card };
    });
    // ==================== 对话历史操作 ====================
    electron_1.ipcMain.handle('dialog:createSession', (_event, characterId) => {
        return dialog_store_1.dialogStore.createSession(characterId);
    });
    electron_1.ipcMain.handle('dialog:getMessages', (_event, sessionId, options) => {
        return dialog_store_1.dialogStore.getSessionMessages(sessionId, options);
    });
    electron_1.ipcMain.handle('dialog:getSessions', (_event, limit) => {
        return dialog_store_1.dialogStore.getRecentSessions(limit);
    });
    electron_1.ipcMain.handle('dialog:deleteSession', (_event, sessionId) => {
        dialog_store_1.dialogStore.deleteSession(sessionId);
    });
    // ==================== 进程管理 ====================
    electron_1.ipcMain.handle('process:getStatus', (_event, processId) => {
        return process_manager_1.processManager.getProcessStatus(processId);
    });
    electron_1.ipcMain.handle('process:getAllStatuses', () => {
        return process_manager_1.processManager.getAllStatuses();
    });
    electron_1.ipcMain.handle('process:restart', async (_event, processId) => {
        await process_manager_1.processManager.restartProcess(processId);
    });
    // ==================== 应用信息 ====================
    electron_1.ipcMain.handle('app:getVersion', () => {
        return electron_1.app.getVersion();
    });
    electron_1.ipcMain.handle('app:getPlatform', () => {
        return process.platform;
    });
    electron_1.ipcMain.handle('app:getConfigDir', () => {
        return electron_1.app.getPath('userData');
    });
    electron_1.ipcMain.handle('app:getCharactersDir', () => {
        return character_store_1.characterStore.getCharactersDir();
    });
}
/**
 * 应用退出前清理
 */
function handleBeforeQuit() {
    console.log('[Main] 应用退出，清理资源...');
    // 停止所有子进程
    process_manager_1.processManager.stopAll();
    // 销毁 IPC Router
    ipc_router_1.ipcRouter.dispose();
    // 销毁窗口管理器
    window_manager_1.windowManager.setQuitting(true);
    window_manager_1.windowManager.dispose();
    // 销毁托盘
    native_bridge_1.nativeBridge.destroyTray();
    // 关闭数据库连接
    dialog_store_1.dialogStore.close();
    console.log('[Main] 资源清理完成');
}
// ==================== Electron 应用事件 ====================
electron_1.app.whenReady().then(async () => {
    try {
        await initialize();
    }
    catch (err) {
        console.error('[Main] 初始化失败:', err);
        // 初始化失败仍然让应用继续运行，不影响已初始化的功能
    }
});
electron_1.app.on('window-all-closed', () => {
    // macOS 上通常不退出应用（Cmd+Q 才退出）
    if (process.platform !== 'darwin') {
        electron_1.app.quit();
    }
});
electron_1.app.on('activate', () => {
    // macOS：点击 Dock 图标时重新创建窗口
    if (window_manager_1.windowManager.getMainWindow() === null) {
        window_manager_1.windowManager.createMainWindow();
    }
    else {
        window_manager_1.windowManager.show();
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
//# sourceMappingURL=index.js.map