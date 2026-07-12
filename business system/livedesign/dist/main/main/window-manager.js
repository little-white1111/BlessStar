"use strict";
/**
 * 窗口管理（架构不变量 #10 — 悬浮球位置变更实时持久化）
 *
 * 职责：
 * - 创建 / 销毁主窗口
 * - 管理窗口状态（置顶、位置恢复与实时持久化）
 * - 系统托盘集成
 * - 窗口位置 & 状态变化实时写入配置存储
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
exports.windowManager = exports.WindowManager = void 0;
const electron_1 = require("electron");
const path = __importStar(require("path"));
const ipc_router_1 = require("./ipc-router");
const config_store_1 = require("./storage/config-store");
/** 窗口配置默认值 */
const DEFAULT_WINDOW_CONFIG = {
    width: 1200,
    height: 800,
    minWidth: 800,
    minHeight: 600,
};
/** 悬浮球配置键 */
const FLOAT_BALL_X_KEY = 'ui.floatBall.x';
const FLOAT_BALL_Y_KEY = 'ui.floatBall.y';
const FLOAT_BALL_ENABLED_KEY = 'ui.floatBall.enabled';
const WINDOW_X_KEY = 'window.x';
const WINDOW_Y_KEY = 'window.y';
const WINDOW_WIDTH_KEY = 'window.width';
const WINDOW_HEIGHT_KEY = 'window.height';
const WINDOW_MAXIMIZED_KEY = 'window.maximized';
const WINDOW_ALWAYS_ON_TOP_KEY = 'window.alwaysOnTop';
class WindowManager {
    mainWindow = null;
    isQuitting = false;
    /**
     * 创建主窗口
     */
    createMainWindow() {
        // 恢复上次的窗口位置和大小
        const savedX = config_store_1.configStore.get(WINDOW_X_KEY);
        const savedY = config_store_1.configStore.get(WINDOW_Y_KEY);
        const savedWidth = config_store_1.configStore.get(WINDOW_WIDTH_KEY);
        const savedHeight = config_store_1.configStore.get(WINDOW_HEIGHT_KEY);
        const savedAlwaysOnTop = config_store_1.configStore.get(WINDOW_ALWAYS_ON_TOP_KEY);
        const windowOptions = {
            width: savedWidth ?? DEFAULT_WINDOW_CONFIG.width,
            height: savedHeight ?? DEFAULT_WINDOW_CONFIG.height,
            minWidth: DEFAULT_WINDOW_CONFIG.minWidth,
            minHeight: DEFAULT_WINDOW_CONFIG.minHeight,
            alwaysOnTop: savedAlwaysOnTop ?? false,
            show: false, // 先不显示，等 ready-to-show 再显示
            webPreferences: {
                preload: path.join(__dirname, '..', 'preload', 'index.js'),
                contextIsolation: true,
                nodeIntegration: false,
                sandbox: false,
                webSecurity: true,
            },
            icon: path.join(__dirname, '..', '..', 'resources', 'icon.png'),
            frame: true,
            titleBarStyle: 'default',
        };
        // 如果有保存的位置且在屏幕范围内，则恢复位置
        if (savedX !== undefined && savedY !== undefined) {
            const displays = electron_1.screen.getAllDisplays();
            const bounds = displays.reduce((acc, display) => {
                const { x, y, width, height } = display.bounds;
                return {
                    minX: Math.min(acc.minX, x),
                    minY: Math.min(acc.minY, y),
                    maxX: Math.max(acc.maxX, x + width),
                    maxY: Math.max(acc.maxY, y + height),
                };
            }, { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity });
            // 检查保存的位置是否在任意显示器范围内
            if (savedX >= bounds.minX && savedX < bounds.maxX &&
                savedY >= bounds.minY && savedY < bounds.maxY) {
                windowOptions.x = savedX;
                windowOptions.y = savedY;
            }
        }
        this.mainWindow = new electron_1.BrowserWindow(windowOptions);
        // 注册 IPC 监听
        this.setupIpcListeners();
        // 窗口事件监听
        this.setupWindowEventListeners();
        // 在开发模式下加载 Vite 开发服务器
        if (process.env.NODE_ENV === 'development' || process.env.VITE_DEV_SERVER_URL) {
            const devUrl = process.env.VITE_DEV_SERVER_URL || 'http://localhost:5173';
            this.mainWindow.loadURL(devUrl);
            this.mainWindow.webContents.openDevTools();
        }
        else {
            // 生产模式下加载打包后的文件
            this.mainWindow.loadFile(path.join(__dirname, '..', 'renderer', 'index.html'));
        }
        // ready-to-show 时显示窗口，避免白屏闪烁
        this.mainWindow.once('ready-to-show', () => {
            if (this.mainWindow && !this.mainWindow.isDestroyed()) {
                this.mainWindow.show();
                // 恢复最大化状态
                const wasMaximized = config_store_1.configStore.get(WINDOW_MAXIMIZED_KEY);
                if (wasMaximized) {
                    this.mainWindow.maximize();
                }
            }
        });
        // 将主窗口注册到 IPC Router
        ipc_router_1.ipcRouter.setMainWindow(this.mainWindow);
        return this.mainWindow;
    }
    /**
     * 设置 IPC 监听（响应 Renderer 的窗口操作请求）
     */
    setupIpcListeners() {
        // 注：Renderer 通过 IPC Router 发送窗口操作请求，
        // 这些 handler 在 main 进程处理
        const { ipcMain } = require('electron');
        ipcMain.handle('window:minimize', () => {
            this.minimize();
        });
        ipcMain.handle('window:maximize', () => {
            this.toggleMaximize();
        });
        ipcMain.handle('window:close', () => {
            this.close();
        });
        ipcMain.handle('window:isMaximized', () => {
            return this.mainWindow?.isMaximized() ?? false;
        });
        ipcMain.handle('window:setAlwaysOnTop', (_event, alwaysOnTop) => {
            this.setAlwaysOnTop(alwaysOnTop);
        });
        ipcMain.handle('window:getAlwaysOnTop', () => {
            return this.mainWindow?.isAlwaysOnTop() ?? false;
        });
    }
    /**
     * 设置窗口事件监听（位置/大小变化时实时持久化）
     *
     * 架构不变量 #10：悬浮球位置变更实时持久化
     * 窗口位置变化也实时持久化
     */
    setupWindowEventListeners() {
        if (!this.mainWindow)
            return;
        // 窗口移动结束时持久化位置
        this.mainWindow.on('moved', () => {
            this.persistWindowBounds();
        });
        // 窗口大小变化结束时持久化
        this.mainWindow.on('resized', () => {
            this.persistWindowBounds();
        });
        // 窗口最大化/还原状态变化
        this.mainWindow.on('maximize', () => {
            config_store_1.configStore.set(WINDOW_MAXIMIZED_KEY, true);
        });
        this.mainWindow.on('unmaximize', () => {
            config_store_1.configStore.set(WINDOW_MAXIMIZED_KEY, false);
        });
        // 窗口关闭前持久化
        this.mainWindow.on('close', () => {
            this.persistWindowBounds();
        });
        // 窗口关闭时清理
        this.mainWindow.on('closed', () => {
            this.mainWindow = null;
        });
    }
    /**
     * 持久化窗口位置和大小
     * 架构不变量 #10：实时持久化
     */
    persistWindowBounds() {
        if (!this.mainWindow || this.mainWindow.isDestroyed())
            return;
        const bounds = this.mainWindow.getBounds();
        config_store_1.configStore.set(WINDOW_X_KEY, bounds.x);
        config_store_1.configStore.set(WINDOW_Y_KEY, bounds.y);
        config_store_1.configStore.set(WINDOW_WIDTH_KEY, bounds.width);
        config_store_1.configStore.set(WINDOW_HEIGHT_KEY, bounds.height);
    }
    // ==================== 浮球位置持久化（架构不变量 #10） ====================
    /**
     * 保存悬浮球位置（由 Renderer 实时调用）
     */
    saveFloatBallPosition(x, y) {
        config_store_1.configStore.set(FLOAT_BALL_X_KEY, x);
        config_store_1.configStore.set(FLOAT_BALL_Y_KEY, y);
    }
    /**
     * 获取保存的悬浮球位置
     */
    getFloatBallPosition() {
        const x = config_store_1.configStore.get(FLOAT_BALL_X_KEY);
        const y = config_store_1.configStore.get(FLOAT_BALL_Y_KEY);
        if (x === undefined || y === undefined)
            return null;
        return { x, y };
    }
    /**
     * 设置悬浮球启用状态
     */
    setFloatBallEnabled(enabled) {
        config_store_1.configStore.set(FLOAT_BALL_ENABLED_KEY, enabled);
        // 同步通知 Renderer
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            this.mainWindow.webContents.send('floatball:enabled', enabled);
        }
    }
    /**
     * 获取悬浮球启用状态
     */
    getFloatBallEnabled() {
        return config_store_1.configStore.get(FLOAT_BALL_ENABLED_KEY) ?? true;
    }
    // ==================== 窗口操作 ====================
    /**
     * 最小化窗口
     */
    minimize() {
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            this.mainWindow.minimize();
        }
    }
    /**
     * 切换最大化/还原
     */
    toggleMaximize() {
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            if (this.mainWindow.isMaximized()) {
                this.mainWindow.unmaximize();
            }
            else {
                this.mainWindow.maximize();
            }
        }
    }
    /**
     * 关闭窗口（可能隐藏到托盘而非退出）
     */
    close() {
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            // 如果启用了托盘且不是真正退出，隐藏到托盘
            const minimizeToTray = config_store_1.configStore.get('ui.minimizeToTray');
            if (minimizeToTray && !this.isQuitting) {
                this.mainWindow.hide();
            }
            else {
                this.mainWindow.close();
            }
        }
    }
    /**
     * 设置窗口置顶
     */
    setAlwaysOnTop(alwaysOnTop) {
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            this.mainWindow.setAlwaysOnTop(alwaysOnTop);
            config_store_1.configStore.set(WINDOW_ALWAYS_ON_TOP_KEY, alwaysOnTop);
        }
    }
    /**
     * 显示窗口
     */
    show() {
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            if (this.mainWindow.isMinimized()) {
                this.mainWindow.restore();
            }
            this.mainWindow.show();
            this.mainWindow.focus();
        }
    }
    /**
     * 获取主窗口实例
     */
    getMainWindow() {
        return this.mainWindow;
    }
    /**
     * 设置退出标志（防止关闭时隐藏到托盘）
     */
    setQuitting(quitting) {
        this.isQuitting = quitting;
    }
    /**
     * 销毁窗口管理器
     */
    dispose() {
        this.isQuitting = true;
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            this.persistWindowBounds();
            this.mainWindow.destroy();
        }
        this.mainWindow = null;
    }
}
exports.WindowManager = WindowManager;
/** 全局单例 */
exports.windowManager = new WindowManager();
//# sourceMappingURL=window-manager.js.map