/**
 * 窗口管理（架构不变量 #10 — 悬浮球位置变更实时持久化）
 *
 * 职责：
 * - 创建 / 销毁主窗口
 * - 管理窗口状态（置顶、位置恢复与实时持久化）
 * - 系统托盘集成
 * - 窗口位置 & 状态变化实时写入配置存储
 */
import { BrowserWindow } from 'electron';
export declare class WindowManager {
    private mainWindow;
    private isQuitting;
    /**
     * 创建主窗口
     */
    createMainWindow(): BrowserWindow;
    /**
     * 设置 IPC 监听（响应 Renderer 的窗口操作请求）
     */
    private setupIpcListeners;
    /**
     * 设置窗口事件监听（位置/大小变化时实时持久化）
     *
     * 架构不变量 #10：悬浮球位置变更实时持久化
     * 窗口位置变化也实时持久化
     */
    private setupWindowEventListeners;
    /**
     * 持久化窗口位置和大小
     * 架构不变量 #10：实时持久化
     */
    private persistWindowBounds;
    /**
     * 保存悬浮球位置（由 Renderer 实时调用）
     */
    saveFloatBallPosition(x: number, y: number): void;
    /**
     * 获取保存的悬浮球位置
     */
    getFloatBallPosition(): {
        x: number;
        y: number;
    } | null;
    /**
     * 设置悬浮球启用状态
     */
    setFloatBallEnabled(enabled: boolean): void;
    /**
     * 获取悬浮球启用状态
     */
    getFloatBallEnabled(): boolean;
    /**
     * 最小化窗口
     */
    minimize(): void;
    /**
     * 切换最大化/还原
     */
    toggleMaximize(): void;
    /**
     * 关闭窗口（可能隐藏到托盘而非退出）
     */
    close(): void;
    /**
     * 设置窗口置顶
     */
    setAlwaysOnTop(alwaysOnTop: boolean): void;
    /**
     * 显示窗口
     */
    show(): void;
    /**
     * 获取主窗口实例
     */
    getMainWindow(): BrowserWindow | null;
    /**
     * 设置退出标志（防止关闭时隐藏到托盘）
     */
    setQuitting(quitting: boolean): void;
    /**
     * 销毁窗口管理器
     */
    dispose(): void;
}
/** 全局单例 */
export declare const windowManager: WindowManager;
//# sourceMappingURL=window-manager.d.ts.map