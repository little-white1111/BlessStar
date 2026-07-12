/**
 * 窗口管理（架构不变量 #10 — 悬浮球位置变更实时持久化）
 *
 * 职责：
 * - 创建 / 销毁主窗口
 * - 管理窗口状态（置顶、位置恢复与实时持久化）
 * - 系统托盘集成
 * - 窗口位置 & 状态变化实时写入配置存储
 */

import { BrowserWindow, screen, app } from 'electron';
import * as path from 'path';
import { ipcRouter } from './ipc-router';
import { nativeBridge } from './native-bridge';
import { configStore } from './storage/config-store';

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

export class WindowManager {
  private mainWindow: BrowserWindow | null = null;
  private isQuitting = false;

  /**
   * 创建主窗口
   */
  createMainWindow(): BrowserWindow {
    // 恢复上次的窗口位置和大小
    const savedX = configStore.get<number>(WINDOW_X_KEY);
    const savedY = configStore.get<number>(WINDOW_Y_KEY);
    const savedWidth = configStore.get<number>(WINDOW_WIDTH_KEY);
    const savedHeight = configStore.get<number>(WINDOW_HEIGHT_KEY);
    const savedAlwaysOnTop = configStore.get<boolean>(WINDOW_ALWAYS_ON_TOP_KEY);

    const windowOptions: Electron.BrowserWindowConstructorOptions = {
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
      const displays = screen.getAllDisplays();
      const bounds = displays.reduce(
        (acc, display) => {
          const { x, y, width, height } = display.bounds;
          return {
            minX: Math.min(acc.minX, x),
            minY: Math.min(acc.minY, y),
            maxX: Math.max(acc.maxX, x + width),
            maxY: Math.max(acc.maxY, y + height),
          };
        },
        { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity },
      );

      // 检查保存的位置是否在任意显示器范围内
      if (savedX >= bounds.minX && savedX < bounds.maxX &&
          savedY >= bounds.minY && savedY < bounds.maxY) {
        windowOptions.x = savedX;
        windowOptions.y = savedY;
      }
    }

    this.mainWindow = new BrowserWindow(windowOptions);

    // 注册 IPC 监听
    this.setupIpcListeners();

    // 窗口事件监听
    this.setupWindowEventListeners();

    // 在开发模式下加载 Vite 开发服务器
    if (process.env.NODE_ENV === 'development' || process.env.VITE_DEV_SERVER_URL) {
      const devUrl = process.env.VITE_DEV_SERVER_URL || 'http://localhost:5173';
      this.mainWindow.loadURL(devUrl);
      this.mainWindow.webContents.openDevTools();
    } else {
      // 生产模式下加载打包后的文件
      this.mainWindow.loadFile(path.join(__dirname, '..', 'renderer', 'index.html'));
    }

    // ready-to-show 时显示窗口，避免白屏闪烁
    this.mainWindow.once('ready-to-show', () => {
      if (this.mainWindow && !this.mainWindow.isDestroyed()) {
        this.mainWindow.show();

        // 恢复最大化状态
        const wasMaximized = configStore.get<boolean>(WINDOW_MAXIMIZED_KEY);
        if (wasMaximized) {
          this.mainWindow.maximize();
        }
      }
    });

    // 将主窗口注册到 IPC Router
    ipcRouter.setMainWindow(this.mainWindow);

    return this.mainWindow;
  }

  /**
   * 设置 IPC 监听（响应 Renderer 的窗口操作请求）
   */
  private setupIpcListeners(): void {
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

    ipcMain.handle('window:setAlwaysOnTop', (_event: Electron.IpcMainInvokeEvent, alwaysOnTop: boolean) => {
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
  private setupWindowEventListeners(): void {
    if (!this.mainWindow) return;

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
      configStore.set(WINDOW_MAXIMIZED_KEY, true);
    });

    this.mainWindow.on('unmaximize', () => {
      configStore.set(WINDOW_MAXIMIZED_KEY, false);
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
  private persistWindowBounds(): void {
    if (!this.mainWindow || this.mainWindow.isDestroyed()) return;

    const bounds = this.mainWindow.getBounds();
    configStore.set(WINDOW_X_KEY, bounds.x);
    configStore.set(WINDOW_Y_KEY, bounds.y);
    configStore.set(WINDOW_WIDTH_KEY, bounds.width);
    configStore.set(WINDOW_HEIGHT_KEY, bounds.height);
  }

  // ==================== 浮球位置持久化（架构不变量 #10） ====================

  /**
   * 保存悬浮球位置（由 Renderer 实时调用）
   */
  saveFloatBallPosition(x: number, y: number): void {
    configStore.set(FLOAT_BALL_X_KEY, x);
    configStore.set(FLOAT_BALL_Y_KEY, y);
  }

  /**
   * 获取保存的悬浮球位置
   */
  getFloatBallPosition(): { x: number; y: number } | null {
    const x = configStore.get<number>(FLOAT_BALL_X_KEY);
    const y = configStore.get<number>(FLOAT_BALL_Y_KEY);
    if (x === undefined || y === undefined) return null;
    return { x, y };
  }

  /**
   * 设置悬浮球启用状态
   */
  setFloatBallEnabled(enabled: boolean): void {
    configStore.set(FLOAT_BALL_ENABLED_KEY, enabled);
    // 同步通知 Renderer
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      this.mainWindow.webContents.send('floatball:enabled', enabled);
    }
  }

  /**
   * 获取悬浮球启用状态
   */
  getFloatBallEnabled(): boolean {
    return configStore.get<boolean>(FLOAT_BALL_ENABLED_KEY) ?? true;
  }

  // ==================== 窗口操作 ====================

  /**
   * 最小化窗口
   */
  minimize(): void {
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      this.mainWindow.minimize();
    }
  }

  /**
   * 切换最大化/还原
   */
  toggleMaximize(): void {
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      if (this.mainWindow.isMaximized()) {
        this.mainWindow.unmaximize();
      } else {
        this.mainWindow.maximize();
      }
    }
  }

  /**
   * 关闭窗口（可能隐藏到托盘而非退出）
   */
  close(): void {
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      // 如果启用了托盘且不是真正退出，隐藏到托盘
      const minimizeToTray = configStore.get<boolean>('ui.minimizeToTray');
      if (minimizeToTray && !this.isQuitting) {
        this.mainWindow.hide();
      } else {
        this.mainWindow.close();
      }
    }
  }

  /**
   * 设置窗口置顶
   */
  setAlwaysOnTop(alwaysOnTop: boolean): void {
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      this.mainWindow.setAlwaysOnTop(alwaysOnTop);
      configStore.set(WINDOW_ALWAYS_ON_TOP_KEY, alwaysOnTop);
    }
  }

  /**
   * 显示窗口
   */
  show(): void {
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
  getMainWindow(): BrowserWindow | null {
    return this.mainWindow;
  }

  /**
   * 设置退出标志（防止关闭时隐藏到托盘）
   */
  setQuitting(quitting: boolean): void {
    this.isQuitting = quitting;
  }

  /**
   * 销毁窗口管理器
   */
  dispose(): void {
    this.isQuitting = true;
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      this.persistWindowBounds();
      this.mainWindow.destroy();
    }
    this.mainWindow = null;
  }
}

/** 全局单例 */
export const windowManager = new WindowManager();
