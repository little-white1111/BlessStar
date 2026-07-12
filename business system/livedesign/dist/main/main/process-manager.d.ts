/**
 * 子进程管理（架构不变量 #4 — LLM Service 崩溃不影响 UI，主进程监听并自动重启）
 *
 * 职责：
 * - 使用 child_process.fork() 启动 LLM Service 和 Plugin Host 子进程
 * - 进程退出自动重启（最多 3 次）
 * - 进程间 IPC 桥接（将 child process message 转发到 IpcRouter）
 */
import { ProcessId } from '../shared/ipc-protocol';
export declare class ProcessManager {
    /** 所有托管的子进程运行时 */
    private processes;
    /** 应用是否正在退出 */
    private isShuttingDown;
    /**
     * 构建子进程入口脚本的绝对路径
     */
    private resolveScriptPath;
    /**
     * 启动 LLM Service 子进程
     */
    startLlmService(): Promise<void>;
    /**
     * 启动 Plugin Host 子进程
     */
    startPluginHost(): Promise<void>;
    /**
     * 启动单个子进程
     */
    private startProcess;
    /**
     * 处理来自子进程的 IPC 消息
     */
    private handleChildMessage;
    /**
     * 处理子进程退出事件
     * 架构不变量 #4：LLM Service 崩溃不影响 UI，主进程监听并自动重启
     */
    private handleProcessExit;
    /**
     * 通知 Renderer 子进程已就绪
     */
    private notifyProcessReady;
    /**
     * 通知 Renderer 子进程已停止
     */
    private notifyProcessDown;
    /**
     * 手动停止指定子进程
     */
    stopProcess(processId: ProcessId): void;
    /**
     * 重启指定子进程
     */
    restartProcess(processId: ProcessId): Promise<void>;
    /**
     * 获取子进程运行状态
     */
    getProcessStatus(processId: ProcessId): {
        running: boolean;
        restartCount: number;
        pid: number | undefined;
    };
    /**
     * 判断消息是否为 IpcEnvelope 格式
     */
    private isIpcEnvelope;
    /**
     * 停止所有子进程
     */
    stopAll(): void;
    /**
     * 获取所有进程的运行状态
     */
    getAllStatuses(): Array<{
        name: string;
        processId: ProcessId;
        running: boolean;
        restartCount: number;
        pid: number | undefined;
    }>;
}
/** 全局单例 */
export declare const processManager: ProcessManager;
//# sourceMappingURL=process-manager.d.ts.map