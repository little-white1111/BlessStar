"use strict";
/**
 * 原生 API 桥接
 *
 * 为 Renderer 进程提供安全的原生能力访问：
 * - 文件对话框（打开文件 / 保存文件）
 * - 系统通知
 * - 系统托盘
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.nativeBridge = exports.NativeBridge = void 0;
const electron_1 = require("electron");
const fs = __importStar(require("fs"));
class NativeBridge {
    tray = null;
    mainWindow = null;
    /**
     * 初始化原生桥接，注册 IPC 处理器
     */
    init(mainWindow) {
        this.mainWindow = mainWindow;
        this.registerIpcHandlers();
    }
    /**
     * 注册与 Renderer 通信的 IPC handler
     */
    registerIpcHandlers() {
        // 由于架构不变量 #1，Renderer 通过 ipcRouter 发送消息，
        // 我们在 ipcMain 上监听来自 Renderer 的消息
        electron_1.ipcMain.on('ipc-message', (event, envelope) => {
            // ipcRouter.handleRendererMessage 已经处理了路由，
            // 这里只处理需要主进程直接处理的消息
            // 其他消息由 ipcRouter 统一路由
        });
    }
    // ==================== 文件对话框 ====================
    /**
     * 打开文件选择对话框
     */
    async showOpenDialog(options = {}) {
        const win = this.mainWindow;
        if (!win || win.isDestroyed()) {
            throw new Error('主窗口不可用，无法打开对话框');
        }
        const result = await electron_1.dialog.showOpenDialog(win, {
            title: options.title || '选择文件',
            defaultPath: options.defaultPath,
            filters: options.filters,
            properties: options.multiSelections ? ['openFile', 'multiSelections'] : ['openFile'],
        });
        return {
            canceled: result.canceled,
            filePaths: result.filePaths,
        };
    }
    /**
     * 打开保存文件对话框
     */
    async showSaveDialog(options = {}) {
        const win = this.mainWindow;
        if (!win || win.isDestroyed()) {
            throw new Error('主窗口不可用，无法打开对话框');
        }
        const result = await electron_1.dialog.showSaveDialog(win, {
            title: options.title || '保存文件',
            defaultPath: options.defaultPath,
            filters: options.filters,
        });
        if (result.canceled || !result.filePath) {
            return null;
        }
        return result.filePath;
    }
    /**
     * 读取文件内容（供 Plugin Host 能力请求使用）
     */
    readFile(filePath) {
        return fs.readFileSync(filePath, 'utf-8');
    }
    /**
     * 写入文件内容（供 Plugin Host 能力请求使用）
     */
    writeFile(filePath, content) {
        fs.writeFileSync(filePath, content, 'utf-8');
    }
    // ==================== 系统通知 ====================
    /**
     * 显示系统通知
     */
    showNotification(title, body, onClick) {
        const notification = new electron_1.Notification({
            title,
            body,
            silent: false,
        });
        if (onClick) {
            notification.on('click', onClick);
        }
        notification.show();
        return notification;
    }
    // ==================== 系统托盘 ====================
    /**
     * 创建系统托盘图标和菜单
     */
    createTray(iconPath) {
        // 如果托盘图标文件不存在，创建一个 16x16 的空白图标
        let icon;
        try {
            if (fs.existsSync(iconPath)) {
                icon = electron_1.nativeImage.createFromPath(iconPath);
            }
            else {
                // 创建一个 16x16 的空白图片作为 fallback
                icon = electron_1.nativeImage.createEmpty();
            }
        }
        catch {
            icon = electron_1.nativeImage.createEmpty();
        }
        this.tray = new electron_1.Tray(icon);
        this.tray.setToolTip('LiveDesign');
        // 构建默认托盘菜单
        const contextMenu = electron_1.Menu.buildFromTemplate([
            {
                label: '显示主窗口',
                click: () => {
                    this.showMainWindow();
                },
            },
            {
                label: '置顶切换',
                type: 'checkbox',
                checked: false,
                click: (menuItem) => {
                    this.toggleAlwaysOnTop(menuItem.checked);
                },
            },
            { type: 'separator' },
            {
                label: '退出',
                click: () => {
                    this.quitApp();
                },
            },
        ]);
        this.tray.setContextMenu(contextMenu);
        // 左键点击显示主窗口
        this.tray.on('click', () => {
            this.showMainWindow();
        });
        return this.tray;
    }
    /**
     * 更新托盘菜单
     */
    updateTrayMenu(menu) {
        if (this.tray) {
            this.tray.setContextMenu(menu);
        }
    }
    /**
     * 销毁托盘
     */
    destroyTray() {
        if (this.tray) {
            this.tray.destroy();
            this.tray = null;
        }
    }
    /**
     * 获取托盘实例
     */
    getTray() {
        return this.tray;
    }
    // ==================== 窗口辅助 ====================
    /**
     * 显示主窗口（从托盘恢复）
     */
    showMainWindow() {
        if (this.mainWindow) {
            if (this.mainWindow.isMinimized()) {
                this.mainWindow.restore();
            }
            if (!this.mainWindow.isVisible()) {
                this.mainWindow.show();
            }
            this.mainWindow.focus();
        }
    }
    /**
     * 切换窗口置顶状态
     */
    toggleAlwaysOnTop(alwaysOnTop) {
        if (this.mainWindow) {
            this.mainWindow.setAlwaysOnTop(alwaysOnTop);
        }
    }
    /**
     * 退出应用
     */
    quitApp() {
        const { app } = require('electron');
        app.quit();
    }
}
exports.NativeBridge = NativeBridge;
/** 全局单例 */
exports.nativeBridge = new NativeBridge();
//# sourceMappingURL=native-bridge.js.map