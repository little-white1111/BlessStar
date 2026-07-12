/**
 * 原生 API 桥接
 *
 * 为 Renderer 进程提供安全的原生能力访问：
 * - 文件对话框（打开文件 / 保存文件）
 * - 系统通知
 * - 系统托盘
 */
import { BrowserWindow, Notification, Tray, Menu } from 'electron';
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
export declare class NativeBridge {
    private tray;
    private mainWindow;
    /**
     * 初始化原生桥接，注册 IPC 处理器
     */
    init(mainWindow: BrowserWindow): void;
    /**
     * 注册与 Renderer 通信的 IPC handler
     */
    private registerIpcHandlers;
    /**
     * 打开文件选择对话框
     */
    showOpenDialog(options?: OpenDialogOptions): Promise<DialogResult>;
    /**
     * 打开保存文件对话框
     */
    showSaveDialog(options?: SaveDialogOptions): Promise<string | null>;
    /**
     * 读取文件内容（供 Plugin Host 能力请求使用）
     */
    readFile(filePath: string): string;
    /**
     * 写入文件内容（供 Plugin Host 能力请求使用）
     */
    writeFile(filePath: string, content: string): void;
    /**
     * 显示系统通知
     */
    showNotification(title: string, body: string, onClick?: () => void): Notification;
    /**
     * 创建系统托盘图标和菜单
     */
    createTray(iconPath: string): Tray;
    /**
     * 更新托盘菜单
     */
    updateTrayMenu(menu: Menu): void;
    /**
     * 销毁托盘
     */
    destroyTray(): void;
    /**
     * 获取托盘实例
     */
    getTray(): Tray | null;
    /**
     * 显示主窗口（从托盘恢复）
     */
    private showMainWindow;
    /**
     * 切换窗口置顶状态
     */
    private toggleAlwaysOnTop;
    /**
     * 退出应用
     */
    private quitApp;
}
/** 全局单例 */
export declare const nativeBridge: NativeBridge;
//# sourceMappingURL=native-bridge.d.ts.map