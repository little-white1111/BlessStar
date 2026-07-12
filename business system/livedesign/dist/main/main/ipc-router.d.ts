/**
 * IPC 路由核心（架构不变量 #1 — IPC Router 是唯一消息路由，禁止跨进程直接调用）
 *
 * 职责：
 * - 所有跨进程消息必须经由 IpcRouter 转发
 * - Renderer → IpcRouter → LLM Service / Plugin Host
 * - Child Process → IpcRouter → Renderer
 * - 维护 pending request 映射，支持请求-响应模式
 */
import { BrowserWindow } from 'electron';
import { IpcEnvelope, IpcChannel, ProcessId } from '../shared/ipc-protocol';
export declare class IpcRouter {
    /** 挂起的请求（请求 ID → PendingRequest） */
    private pendingRequests;
    /** 已注册的子进程引用（ProcessId → BrowserWindow | child process send handle） */
    private childProcessWindows;
    /** 已注册的子进程 send 函数（ProcessId → (msg: unknown) => boolean） */
    private childProcessSenders;
    /** 主窗口引用 */
    private mainWindow;
    /**
     * 设置主窗口引用
     */
    setMainWindow(win: BrowserWindow): void;
    /**
     * 注册子进程引用
     * @param processId 进程标识
     * @param win 子进程对应的 BrowserWindow（如果没有则为 null）
     */
    registerChildProcess(processId: ProcessId, win: BrowserWindow | null): void;
    /**
     * 注册子进程的 send 函数（用于 child_process.fork() 通信）
     */
    registerChildProcessSender(processId: ProcessId, sendFn: (message: unknown) => boolean): void;
    /**
     * 注销子进程
     */
    unregisterChildProcess(processId: ProcessId): void;
    /**
     * 主进程内部发送消息到任意目标，返回 Promise 等待响应
     * @param target 目标进程
     * @param channel 通道
     * @param payload 载荷
     * @param timeout 超时时间（毫秒）
     */
    send(target: ProcessId, channel: IpcChannel, payload: unknown, timeout?: number): Promise<unknown>;
    /**
     * IPC Router 入口：处理来自 Renderer 进程的消息
     * Renderer 通过 ipcRenderer.send('ipc-message', envelope) 发送
     */
    handleRendererMessage(event: Electron.IpcMainEvent, envelope: IpcEnvelope): void;
    /**
     * IPC Router 入口：处理来自子进程（LLM Service / Plugin Host）的消息
     */
    handleChildProcessMessage(processId: ProcessId, envelope: IpcEnvelope): void;
    /**
     * 投递消息到目标进程
     */
    private deliver;
    /**
     * 投递消息到 Renderer 进程
     */
    private deliverToRenderer;
    /**
     * 投递消息到子进程（LLM Service / Plugin Host）
     */
    private deliverToChildProcess;
    /**
     * 校验信封格式
     */
    private validateEnvelope;
    /**
     * 清理所有 pending request（应用退出时调用）
     */
    dispose(): void;
}
/** 全局单例 */
export declare const ipcRouter: IpcRouter;
//# sourceMappingURL=ipc-router.d.ts.map