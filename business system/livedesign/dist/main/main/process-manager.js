"use strict";
/**
 * 子进程管理（架构不变量 #4 — LLM Service 崩溃不影响 UI，主进程监听并自动重启）
 *
 * 职责：
 * - 使用 child_process.fork() 启动 LLM Service 和 Plugin Host 子进程
 * - 进程退出自动重启（最多 3 次）
 * - 进程间 IPC 桥接（将 child process message 转发到 IpcRouter）
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
exports.processManager = exports.ProcessManager = void 0;
const child_process_1 = require("child_process");
const path = __importStar(require("path"));
const ipc_protocol_1 = require("../shared/ipc-protocol");
const ipc_router_1 = require("./ipc-router");
/** 子进程进程 ID 到 ProcessId 的映射 */
const CHILD_PROCESS_MAP = {
    'llm-service': ipc_protocol_1.ProcessId.LLM_SERVICE,
    'plugin-host': ipc_protocol_1.ProcessId.PLUGIN_HOST,
};
class ProcessManager {
    /** 所有托管的子进程运行时 */
    processes = new Map();
    /** 应用是否正在退出 */
    isShuttingDown = false;
    /**
     * 构建子进程入口脚本的绝对路径
     */
    resolveScriptPath(relativePath) {
        // 在开发模式下，脚本在 src/ 下；生产模式下在 dist/ 下
        const baseDir = process.env.NODE_ENV === 'development'
            ? path.join(__dirname, '..', '..', 'src')
            : path.join(__dirname, '..');
        // 去掉可能的 src/ 前缀
        const cleanPath = relativePath.replace(/^src[/\\]/, '');
        return path.resolve(baseDir, cleanPath);
    }
    /**
     * 启动 LLM Service 子进程
     */
    async startLlmService() {
        const config = {
            name: 'llm-service',
            processId: ipc_protocol_1.ProcessId.LLM_SERVICE,
            scriptPath: this.resolveScriptPath('services/llm-service/index.ts'),
            args: [],
            env: {
                NODE_ENV: process.env.NODE_ENV,
                ...process.env,
            },
        };
        await this.startProcess(config);
    }
    /**
     * 启动 Plugin Host 子进程
     */
    async startPluginHost() {
        const config = {
            name: 'plugin-host',
            processId: ipc_protocol_1.ProcessId.PLUGIN_HOST,
            scriptPath: this.resolveScriptPath('services/plugin-host/index.ts'),
            args: [],
            env: {
                NODE_ENV: process.env.NODE_ENV,
                ...process.env,
            },
        };
        await this.startProcess(config);
    }
    /**
     * 启动单个子进程
     */
    startProcess(config) {
        return new Promise((resolve, reject) => {
            try {
                console.log(`[ProcessManager] 启动 ${config.name} (${config.processId})...`);
                const child = (0, child_process_1.fork)(config.scriptPath, config.args, {
                    env: config.env,
                    stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
                    serialization: 'json',
                });
                const runtime = {
                    config,
                    child,
                    restartCount: 0,
                    maxRestarts: 3,
                    manuallyStopped: false,
                };
                this.processes.set(config.processId, runtime);
                // 注册子进程 send 函数到 IPC Router
                ipc_router_1.ipcRouter.registerChildProcessSender(config.processId, (message) => {
                    if (child.killed || !child.connected) {
                        return false;
                    }
                    return child.send(message);
                });
                // 监听子进程消息
                child.on('message', (message) => {
                    this.handleChildMessage(config.processId, message);
                });
                // 监听子进程退出（架构不变量 #4：自动重启）
                child.on('exit', (code, signal) => {
                    console.log(`[ProcessManager] ${config.name} 退出, code=${code}, signal=${signal}`);
                    this.handleProcessExit(config.processId, code, signal);
                });
                // 监听子进程错误
                child.on('error', (err) => {
                    console.error(`[ProcessManager] ${config.name} 错误:`, err);
                });
                // 子进程 stdout
                if (child.stdout) {
                    child.stdout.on('data', (data) => {
                        const lines = data.toString().trim().split('\n');
                        for (const line of lines) {
                            console.log(`[${config.name}:out] ${line}`);
                        }
                    });
                }
                // 子进程 stderr
                if (child.stderr) {
                    child.stderr.on('data', (data) => {
                        const lines = data.toString().trim().split('\n');
                        for (const line of lines) {
                            console.error(`[${config.name}:err] ${line}`);
                        }
                    });
                }
                resolve();
            }
            catch (err) {
                console.error(`[ProcessManager] 启动 ${config.name} 失败:`, err);
                reject(err);
            }
        });
    }
    /**
     * 处理来自子进程的 IPC 消息
     */
    handleChildMessage(processId, message) {
        // 校验消息是否为 IpcEnvelope 格式
        if (this.isIpcEnvelope(message)) {
            ipc_router_1.ipcRouter.handleChildProcessMessage(processId, message);
        }
        else {
            console.warn(`[ProcessManager] 来自 ${processId} 的消息不是有效的 IpcEnvelope 格式:`, message);
        }
    }
    /**
     * 处理子进程退出事件
     * 架构不变量 #4：LLM Service 崩溃不影响 UI，主进程监听并自动重启
     */
    handleProcessExit(processId, code, _signal) {
        const runtime = this.processes.get(processId);
        if (!runtime)
            return;
        // 清理旧的 send 函数
        ipc_router_1.ipcRouter.unregisterChildProcess(processId);
        // 如果是手动停止或正在关闭，不重启
        if (runtime.manuallyStopped || this.isShuttingDown) {
            console.log(`[ProcessManager] ${runtime.config.name} 已手动停止，不重启`);
            return;
        }
        // 检查重启次数
        if (runtime.restartCount >= runtime.maxRestarts) {
            console.error(`[ProcessManager] ${runtime.config.name} 已重启 ${runtime.restartCount} 次，达到上限，不再重启`);
            // 通知 Renderer 子进程已永久停止
            this.notifyProcessDown(processId, code);
            return;
        }
        // 自动重启
        runtime.restartCount++;
        const restartDelay = Math.min(1000 * runtime.restartCount, 5000); // 指数退避，最大 5 秒
        console.log(`[ProcessManager] ${runtime.config.name} 将在 ${restartDelay}ms 后自动重启 (第 ${runtime.restartCount} 次)`);
        setTimeout(async () => {
            if (this.isShuttingDown)
                return;
            try {
                await this.startProcess(runtime.config);
                console.log(`[ProcessManager] ${runtime.config.name} 自动重启成功`);
                // 通知 Renderer 进程已恢复
                this.notifyProcessReady(processId);
            }
            catch (err) {
                console.error(`[ProcessManager] ${runtime.config.name} 自动重启失败:`, err);
            }
        }, restartDelay);
    }
    /**
     * 通知 Renderer 子进程已就绪
     */
    notifyProcessReady(processId) {
        const channel = processId === ipc_protocol_1.ProcessId.LLM_SERVICE
            ? ipc_protocol_1.IpcChannel.SERVICE_READY
            : ipc_protocol_1.IpcChannel.PLUGIN_HOST_READY;
        // 通过 IPC Router 发送就绪通知
        ipc_router_1.ipcRouter.send(ipc_protocol_1.ProcessId.RENDERER, channel, {
            processId,
            ready: true,
            timestamp: Date.now(),
        }).catch((err) => {
            console.warn(`[ProcessManager] 发送就绪通知失败:`, err);
        });
    }
    /**
     * 通知 Renderer 子进程已停止
     */
    notifyProcessDown(processId, code) {
        const channel = processId === ipc_protocol_1.ProcessId.LLM_SERVICE
            ? ipc_protocol_1.IpcChannel.SERVICE_READY
            : ipc_protocol_1.IpcChannel.PLUGIN_HOST_READY;
        ipc_router_1.ipcRouter.send(ipc_protocol_1.ProcessId.RENDERER, channel, {
            processId,
            ready: false,
            exitCode: code,
            timestamp: Date.now(),
        }).catch((err) => {
            console.warn(`[ProcessManager] 发送停止通知失败:`, err);
        });
    }
    /**
     * 手动停止指定子进程
     */
    stopProcess(processId) {
        const runtime = this.processes.get(processId);
        if (!runtime || !runtime.child)
            return;
        runtime.manuallyStopped = true;
        try {
            if (runtime.child.connected) {
                runtime.child.disconnect();
            }
            runtime.child.kill();
        }
        catch (err) {
            console.error(`[ProcessManager] 停止 ${runtime.config.name} 时出错:`, err);
        }
        ipc_router_1.ipcRouter.unregisterChildProcess(processId);
    }
    /**
     * 重启指定子进程
     */
    async restartProcess(processId) {
        const runtime = this.processes.get(processId);
        if (!runtime) {
            throw new Error(`[ProcessManager] 未知的进程: ${processId}`);
        }
        // 停止旧进程
        this.stopProcess(processId);
        // 重置重启计数
        runtime.manuallyStopped = false;
        runtime.restartCount = 0;
        // 启动新进程
        await this.startProcess(runtime.config);
    }
    /**
     * 获取子进程运行状态
     */
    getProcessStatus(processId) {
        const runtime = this.processes.get(processId);
        if (!runtime) {
            return { running: false, restartCount: 0, pid: undefined };
        }
        return {
            running: runtime.child !== null && !runtime.child.killed && runtime.child.connected === true,
            restartCount: runtime.restartCount,
            pid: runtime.child?.pid,
        };
    }
    /**
     * 判断消息是否为 IpcEnvelope 格式
     */
    isIpcEnvelope(message) {
        if (!message || typeof message !== 'object')
            return false;
        const m = message;
        return (typeof m.id === 'string' &&
            typeof m.channel === 'string' &&
            Object.values(ipc_protocol_1.IpcChannel).includes(m.channel));
    }
    /**
     * 停止所有子进程
     */
    stopAll() {
        this.isShuttingDown = true;
        for (const processId of this.processes.keys()) {
            this.stopProcess(processId);
        }
        this.processes.clear();
    }
    /**
     * 获取所有进程的运行状态
     */
    getAllStatuses() {
        const statuses = [];
        for (const [processId, _runtime] of this.processes) {
            statuses.push({
                name: _runtime.config.name,
                processId,
                ...this.getProcessStatus(processId),
            });
        }
        return statuses;
    }
}
exports.ProcessManager = ProcessManager;
/** 全局单例 */
exports.processManager = new ProcessManager();
//# sourceMappingURL=process-manager.js.map