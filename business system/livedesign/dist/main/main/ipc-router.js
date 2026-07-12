"use strict";
/**
 * IPC 路由核心（架构不变量 #1 — IPC Router 是唯一消息路由，禁止跨进程直接调用）
 *
 * 职责：
 * - 所有跨进程消息必须经由 IpcRouter 转发
 * - Renderer → IpcRouter → LLM Service / Plugin Host
 * - Child Process → IpcRouter → Renderer
 * - 维护 pending request 映射，支持请求-响应模式
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.ipcRouter = exports.IpcRouter = void 0;
const ipc_protocol_1 = require("../shared/ipc-protocol");
const uuid_1 = require("uuid");
/** IPC 消息超时时间（默认 30 秒） */
const IPC_TIMEOUT_MS = 30_000;
/** 通道 → 目标进程映射 */
const CHANNEL_ROUTE = {
    // LLM 相关消息路由到 LLM Service
    [ipc_protocol_1.IpcChannel.LLM_CHAT]: ipc_protocol_1.ProcessId.LLM_SERVICE,
    [ipc_protocol_1.IpcChannel.LLM_ABORT]: ipc_protocol_1.ProcessId.LLM_SERVICE,
    [ipc_protocol_1.IpcChannel.LLM_SWITCH_CHARACTER]: ipc_protocol_1.ProcessId.LLM_SERVICE,
    // 插件相关消息路由到 Plugin Host
    [ipc_protocol_1.IpcChannel.PLUGIN_INVOKE]: ipc_protocol_1.ProcessId.PLUGIN_HOST,
    [ipc_protocol_1.IpcChannel.PLUGIN_STOP]: ipc_protocol_1.ProcessId.PLUGIN_HOST,
    // 来自子进程的响应路由到 Renderer
    [ipc_protocol_1.IpcChannel.LLM_STREAM_CHUNK]: ipc_protocol_1.ProcessId.RENDERER,
    [ipc_protocol_1.IpcChannel.LLM_RESPONSE]: ipc_protocol_1.ProcessId.RENDERER,
    [ipc_protocol_1.IpcChannel.EMOTION_UPDATE]: ipc_protocol_1.ProcessId.RENDERER,
    [ipc_protocol_1.IpcChannel.PLUGIN_RESULT]: ipc_protocol_1.ProcessId.RENDERER,
    // 插件能力请求路由到 Main Process 内部处理
    [ipc_protocol_1.IpcChannel.PLUGIN_REQUEST_FILE]: ipc_protocol_1.ProcessId.MAIN,
    [ipc_protocol_1.IpcChannel.PLUGIN_REQUEST_NETWORK]: ipc_protocol_1.ProcessId.MAIN,
    [ipc_protocol_1.IpcChannel.PLUGIN_APPROVAL_RESULT]: ipc_protocol_1.ProcessId.PLUGIN_HOST,
    // 广播
    [ipc_protocol_1.IpcChannel.CONFIG_CHANGED]: ipc_protocol_1.ProcessId.RENDERER,
    [ipc_protocol_1.IpcChannel.NOTIFICATION]: ipc_protocol_1.ProcessId.RENDERER,
    // 进程管理
    [ipc_protocol_1.IpcChannel.SERVICE_READY]: ipc_protocol_1.ProcessId.MAIN,
    [ipc_protocol_1.IpcChannel.PLUGIN_HOST_READY]: ipc_protocol_1.ProcessId.MAIN,
};
/**
 * 判断通道对应的目标进程
 */
function resolveTargetProcess(channel) {
    const target = CHANNEL_ROUTE[channel];
    if (!target) {
        throw new Error(`[IpcRouter] 未知的通道: ${channel}，无法确定目标进程`);
    }
    return target;
}
class IpcRouter {
    /** 挂起的请求（请求 ID → PendingRequest） */
    pendingRequests = new Map();
    /** 已注册的子进程引用（ProcessId → BrowserWindow | child process send handle） */
    childProcessWindows = new Map();
    /** 已注册的子进程 send 函数（ProcessId → (msg: unknown) => boolean） */
    childProcessSenders = new Map();
    /** 主窗口引用 */
    mainWindow = null;
    /**
     * 设置主窗口引用
     */
    setMainWindow(win) {
        this.mainWindow = win;
    }
    /**
     * 注册子进程引用
     * @param processId 进程标识
     * @param win 子进程对应的 BrowserWindow（如果没有则为 null）
     */
    registerChildProcess(processId, win) {
        if (processId === ipc_protocol_1.ProcessId.MAIN || processId === ipc_protocol_1.ProcessId.RENDERER) {
            throw new Error(`[IpcRouter] 不允许注册核心进程: ${processId}`);
        }
        this.childProcessWindows.set(processId, win);
    }
    /**
     * 注册子进程的 send 函数（用于 child_process.fork() 通信）
     */
    registerChildProcessSender(processId, sendFn) {
        if (processId === ipc_protocol_1.ProcessId.MAIN || processId === ipc_protocol_1.ProcessId.RENDERER) {
            throw new Error(`[IpcRouter] 不允许注册核心进程: ${processId}`);
        }
        this.childProcessSenders.set(processId, sendFn);
    }
    /**
     * 注销子进程
     */
    unregisterChildProcess(processId) {
        this.childProcessWindows.delete(processId);
        this.childProcessSenders.delete(processId);
    }
    /**
     * 主进程内部发送消息到任意目标，返回 Promise 等待响应
     * @param target 目标进程
     * @param channel 通道
     * @param payload 载荷
     * @param timeout 超时时间（毫秒）
     */
    send(target, channel, payload, timeout = IPC_TIMEOUT_MS) {
        const id = (0, uuid_1.v4)();
        const envelope = {
            id,
            channel,
            payload,
            source: ipc_protocol_1.ProcessId.MAIN,
        };
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pendingRequests.delete(id);
                reject(new Error(`[IpcRouter] 消息超时: channel=${channel}, target=${target}, id=${id}`));
            }, timeout);
            this.pendingRequests.set(id, { resolve, reject, timeout: timer });
            try {
                this.deliver(target, envelope);
            }
            catch (err) {
                this.pendingRequests.delete(id);
                clearTimeout(timer);
                reject(err);
            }
        });
    }
    /**
     * IPC Router 入口：处理来自 Renderer 进程的消息
     * Renderer 通过 ipcRenderer.send('ipc-message', envelope) 发送
     */
    handleRendererMessage(event, envelope) {
        if (!this.validateEnvelope(envelope, 'renderer')) {
            return;
        }
        // 注入来源
        envelope.source = ipc_protocol_1.ProcessId.RENDERER;
        try {
            const target = resolveTargetProcess(envelope.channel);
            this.deliver(target, envelope);
        }
        catch (err) {
            console.error(`[IpcRouter] 路由 Renderer 消息失败:`, err);
            // 如果存在 pending request，reject 它
            const pending = this.pendingRequests.get(envelope.id);
            if (pending) {
                this.pendingRequests.delete(envelope.id);
                clearTimeout(pending.timeout);
                pending.reject(err);
            }
        }
    }
    /**
     * IPC Router 入口：处理来自子进程（LLM Service / Plugin Host）的消息
     */
    handleChildProcessMessage(processId, envelope) {
        if (!this.validateEnvelope(envelope, processId)) {
            return;
        }
        // 注入来源
        envelope.source = processId;
        // 检查是否有 pending request 匹配该消息 ID
        const pending = this.pendingRequests.get(envelope.id);
        if (pending) {
            this.pendingRequests.delete(envelope.id);
            clearTimeout(pending.timeout);
            if (envelope.error) {
                pending.reject(new Error(envelope.error));
            }
            else {
                pending.resolve(envelope.payload);
            }
            return;
        }
        // 无 pending request，按通道路由
        try {
            const target = resolveTargetProcess(envelope.channel);
            this.deliver(target, envelope);
        }
        catch (err) {
            console.error(`[IpcRouter] 路由子进程消息失败: processId=${processId}`, err);
        }
    }
    /**
     * 投递消息到目标进程
     */
    deliver(target, envelope) {
        switch (target) {
            case ipc_protocol_1.ProcessId.MAIN:
                // 主进程内部处理 — 没有需要额外转发的，当前就在主进程
                // 如果存在 pending request，由调用方自行处理
                console.warn(`[IpcRouter] 消息路由到 MAIN（直接消费）: channel=${envelope.channel}`);
                break;
            case ipc_protocol_1.ProcessId.RENDERER:
                this.deliverToRenderer(envelope);
                break;
            case ipc_protocol_1.ProcessId.LLM_SERVICE:
            case ipc_protocol_1.ProcessId.PLUGIN_HOST:
                this.deliverToChildProcess(target, envelope);
                break;
            default:
                throw new Error(`[IpcRouter] 未知的目标进程: ${target}`);
        }
    }
    /**
     * 投递消息到 Renderer 进程
     */
    deliverToRenderer(envelope) {
        const win = this.mainWindow;
        if (!win || win.isDestroyed()) {
            throw new Error('[IpcRouter] 主窗口不存在或已销毁，无法发送消息到 Renderer');
        }
        win.webContents.send('ipc-message', envelope);
    }
    /**
     * 投递消息到子进程（LLM Service / Plugin Host）
     */
    deliverToChildProcess(processId, envelope) {
        const sendFn = this.childProcessSenders.get(processId);
        if (!sendFn) {
            throw new Error(`[IpcRouter] 子进程未注册 send 函数: ${processId}`);
        }
        sendFn(envelope);
    }
    /**
     * 校验信封格式
     */
    validateEnvelope(envelope, sourceLabel) {
        if (!envelope || typeof envelope !== 'object') {
            console.error(`[IpcRouter] 来自 ${sourceLabel} 的消息不是有效对象`);
            return false;
        }
        const e = envelope;
        if (typeof e.id !== 'string' || !e.id) {
            console.error(`[IpcRouter] 来自 ${sourceLabel} 的消息缺少有效 id`);
            return false;
        }
        if (typeof e.channel !== 'string' || !Object.values(ipc_protocol_1.IpcChannel).includes(e.channel)) {
            console.error(`[IpcRouter] 来自 ${sourceLabel} 的消息包含无效 channel: ${e.channel}`);
            return false;
        }
        return true;
    }
    /**
     * 清理所有 pending request（应用退出时调用）
     */
    dispose() {
        for (const [id, pending] of this.pendingRequests) {
            clearTimeout(pending.timeout);
            pending.reject(new Error('[IpcRouter] 路由器已关闭'));
        }
        this.pendingRequests.clear();
        this.childProcessWindows.clear();
        this.childProcessSenders.clear();
        this.mainWindow = null;
    }
}
exports.IpcRouter = IpcRouter;
/** 全局单例 */
exports.ipcRouter = new IpcRouter();
//# sourceMappingURL=ipc-router.js.map