/**
 * 原生 API 桥接
 *
 * 为 Renderer 进程提供安全的原生能力访问：
 * - 文件对话框（打开文件 / 保存文件）
 * - 系统通知
 * - 系统托盘
 */

import { BrowserWindow, dialog, Notification, Tray, nativeImage, Menu, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import { IpcChannel, ProcessId } from '../shared/ipc-protocol';
import { ipcRouter } from './ipc-router';

/** 文件过滤器类型 */
export interface FileFilter {
  name: string;
  extensions: string[];
}

/** 打开文件对话框选项 */
export interface OpenDialogOptions {
  title?: string;
  defaultPath?: string;
  filters?: FileFilter[];
  multiSelections?: boolean;
}

/** 保存文件对话框选项 */
export interface SaveDialogOptions {
  title?: string;
  defaultPath?: string;
  filters?: FileFilter[];
}

/** 对话框结果 */
export interface DialogResult {
  canceled: boolean;
  filePaths: string[];
}

export class NativeBridge {
  private tray: Tray | null = null;
  private mainWindow: BrowserWindow | null = null;

  /**
   * 初始化原生桥接，注册 IPC 处理器
   */
  init(mainWindow: BrowserWindow): void {
    this.mainWindow = mainWindow;
    this.registerIpcHandlers();
  }

  /**
   * 注册与 Renderer 通信的 IPC handler
   */
  private registerIpcHandlers(): void {
    // 由于架构不变量 #1，Renderer 通过 ipcRouter 发送消息，
    // 我们在 ipcMain 上监听来自 Renderer 的消息
    ipcMain.on('ipc-message', (event, envelope) => {
      // ipcRouter.handleRendererMessage 已经处理了路由，
      // 这里只处理需要主进程直接处理的消息
      // 其他消息由 ipcRouter 统一路由
    });
  }

  // ==================== 文件对话框 ====================

  /**
   * 打开文件选择对话框
   */
  async showOpenDialog(options: OpenDialogOptions = {}): Promise<DialogResult> {
    const win = this.mainWindow;
    if (!win || win.isDestroyed()) {
      throw new Error('主窗口不可用，无法打开对话框');
    }

    const result = await dialog.showOpenDialog(win, {
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
  async showSaveDialog(options: SaveDialogOptions = {}): Promise<string | null> {
    const win = this.mainWindow;
    if (!win || win.isDestroyed()) {
      throw new Error('主窗口不可用，无法打开对话框');
    }

    const result = await dialog.showSaveDialog(win, {
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
  readFile(filePath: string): string {
    return fs.readFileSync(filePath, 'utf-8');
  }

  /**
   * 写入文件内容（供 Plugin Host 能力请求使用）
   */
  writeFile(filePath: string, content: string): void {
    fs.writeFileSync(filePath, content, 'utf-8');
  }

  // ==================== 系统通知 ====================

  /**
   * 显示系统通知
   */
  showNotification(title: string, body: string, onClick?: () => void): Notification {
    const notification = new Notification({
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
  createTray(iconPath: string): Tray {
    // 如果托盘图标文件不存在，创建一个 16x16 的空白图标
    let icon: Electron.NativeImage;
    try {
      if (fs.existsSync(iconPath)) {
        icon = nativeImage.createFromPath(iconPath);
      } else {
        // 创建一个 16x16 的空白图片作为 fallback
        icon = nativeImage.createEmpty();
      }
    } catch {
      icon = nativeImage.createEmpty();
    }

    this.tray = new Tray(icon);
    this.tray.setToolTip('LiveDesign');

    // 构建默认托盘菜单
    const contextMenu = Menu.buildFromTemplate([
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
  updateTrayMenu(menu: Menu): void {
    if (this.tray) {
      this.tray.setContextMenu(menu);
    }
  }

  /**
   * 销毁托盘
   */
  destroyTray(): void {
    if (this.tray) {
      this.tray.destroy();
      this.tray = null;
    }
  }

  /**
   * 获取托盘实例
   */
  getTray(): Tray | null {
    return this.tray;
  }

  // ==================== 窗口辅助 ====================

  /**
   * 显示主窗口（从托盘恢复）
   */
  private showMainWindow(): void {
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
  private toggleAlwaysOnTop(alwaysOnTop: boolean): void {
    if (this.mainWindow) {
      this.mainWindow.setAlwaysOnTop(alwaysOnTop);
    }
  }

  /**
   * 退出应用
   */
  private quitApp(): void {
    const { app } = require('electron');
    app.quit();
  }
}

/** 全局单例 */
export const nativeBridge = new NativeBridge();
