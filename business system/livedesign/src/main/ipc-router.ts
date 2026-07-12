/**
 * IPC 路由核心（架构不变量 #1 — IPC Router 是唯一消息路由，禁止跨进程直接调用）
 *
 * 职责：
 * - 所有跨进程消息必须经由 IpcRouter 转发
 * - Renderer → IpcRouter → LLM Service / Plugin Host
 * - Child Process → IpcRouter → Renderer
 * - 维护 pending request 映射，支持请求-响应模式
 */

import { BrowserWindow, ipcMain } from 'electron';
import { IpcEnvelope, IpcChannel, ProcessId, PendingRequest } from '../shared/ipc-protocol';
import { v4 as uuidv4 } from 'uuid';

/** IPC 消息超时时间（默认 30 秒） */
const IPC_TIMEOUT_MS = 30_000;

/** 通道 → 目标进程映射 */
const CHANNEL_ROUTE: Partial<Record<IpcChannel, ProcessId>> = {
  // LLM 相关消息路由到 LLM Service
  [IpcChannel.LLM_CHAT]: ProcessId.LLM_SERVICE,
  [IpcChannel.LLM_ABORT]: ProcessId.LLM_SERVICE,
  [IpcChannel.LLM_SWITCH_CHARACTER]: ProcessId.LLM_SERVICE,

  // 插件相关消息路由到 Plugin Host
  [IpcChannel.PLUGIN_INVOKE]: ProcessId.PLUGIN_HOST,
  [IpcChannel.PLUGIN_STOP]: ProcessId.PLUGIN_HOST,

  // 来自子进程的响应路由到 Renderer
  [IpcChannel.LLM_STREAM_CHUNK]: ProcessId.RENDERER,
  [IpcChannel.LLM_RESPONSE]: ProcessId.RENDERER,
  [IpcChannel.EMOTION_UPDATE]: ProcessId.RENDERER,
  [IpcChannel.PLUGIN_RESULT]: ProcessId.RENDERER,

  // 插件能力请求路由到 Main Process 内部处理
  [IpcChannel.PLUGIN_REQUEST_FILE]: ProcessId.MAIN,
  [IpcChannel.PLUGIN_REQUEST_NETWORK]: ProcessId.MAIN,
  [IpcChannel.PLUGIN_APPROVAL_RESULT]: ProcessId.PLUGIN_HOST,

  // 广播
  [IpcChannel.CONFIG_CHANGED]: ProcessId.RENDERER,
  [IpcChannel.NOTIFICATION]: ProcessId.RENDERER,

  // 进程管理
  [IpcChannel.SERVICE_READY]: ProcessId.MAIN,
  [IpcChannel.PLUGIN_HOST_READY]: ProcessId.MAIN,

  // 口头禅系统
  [IpcChannel.CATCHPHRASE_PROMOTION_REQUEST]: ProcessId.RENDERER,
  [IpcChannel.CATCHPHRASE_PROMOTION_RESULT]: ProcessId.LLM_SERVICE,
};

/**
 * 判断通道对应的目标进程
 */
function resolveTargetProcess(channel: IpcChannel): ProcessId {
  const target = CHANNEL_ROUTE[channel];
  if (!target) {
    throw new Error(`[IpcRouter] 未知的通道: ${channel}，无法确定目标进程`);
  }
  return target;
}

export class IpcRouter {
  /** 挂起的请求（请求 ID → PendingRequest） */
  private pendingRequests = new Map<string, PendingRequest>();

  /** 已注册的子进程引用（ProcessId → BrowserWindow | child process send handle） */
  private childProcessWindows = new Map<ProcessId, BrowserWindow | null>();

  /** 已注册的子进程 send 函数（ProcessId → (msg: unknown) => boolean） */
  private childProcessSenders = new Map<ProcessId, (message: unknown) => boolean>();

  /** 主窗口引用 */
  private mainWindow: BrowserWindow | null = null;

  /**
   * 设置主窗口引用
   */
  setMainWindow(win: BrowserWindow): void {
    this.mainWindow = win;
  }

  /**
   * 注册子进程引用
   * @param processId 进程标识
   * @param win 子进程对应的 BrowserWindow（如果没有则为 null）
   */
  registerChildProcess(processId: ProcessId, win: BrowserWindow | null): void {
    if (processId === ProcessId.MAIN || processId === ProcessId.RENDERER) {
      throw new Error(`[IpcRouter] 不允许注册核心进程: ${processId}`);
    }
    this.childProcessWindows.set(processId, win);
  }

  /**
   * 注册子进程的 send 函数（用于 child_process.fork() 通信）
   */
  registerChildProcessSender(
    processId: ProcessId,
    sendFn: (message: unknown) => boolean,
  ): void {
    if (processId === ProcessId.MAIN || processId === ProcessId.RENDERER) {
      throw new Error(`[IpcRouter] 不允许注册核心进程: ${processId}`);
    }
    this.childProcessSenders.set(processId, sendFn);
  }

  /**
   * 注销子进程
   */
  unregisterChildProcess(processId: ProcessId): void {
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
  send(
    target: ProcessId,
    channel: IpcChannel,
    payload: unknown,
    timeout: number = IPC_TIMEOUT_MS,
  ): Promise<unknown> {
    const id = uuidv4();
    const envelope: IpcEnvelope = {
      id,
      channel,
      payload,
      source: ProcessId.MAIN,
    };

    return new Promise<unknown>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingRequests.delete(id);
        reject(new Error(`[IpcRouter] 消息超时: channel=${channel}, target=${target}, id=${id}`));
      }, timeout);

      this.pendingRequests.set(id, { resolve, reject, timeout: timer });

      try {
        this.deliver(target, envelope);
      } catch (err) {
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
  handleRendererMessage(event: Electron.IpcMainEvent, envelope: IpcEnvelope): void {
    if (!this.validateEnvelope(envelope, 'renderer')) {
      return;
    }

    // 注入来源
    envelope.source = ProcessId.RENDERER;

    try {
      const target = resolveTargetProcess(envelope.channel);
      this.deliver(target, envelope);
    } catch (err) {
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
  handleChildProcessMessage(processId: ProcessId, envelope: IpcEnvelope): void {
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
      } else {
        pending.resolve(envelope.payload);
      }
      return;
    }

    // 无 pending request，按通道路由
    try {
      const target = resolveTargetProcess(envelope.channel);
      this.deliver(target, envelope);
    } catch (err) {
      console.error(`[IpcRouter] 路由子进程消息失败: processId=${processId}`, err);
    }
  }

  /**
   * 投递消息到目标进程
   */
  private deliver(target: ProcessId, envelope: IpcEnvelope): void {
    switch (target) {
      case ProcessId.MAIN:
        // 主进程内部处理 — 没有需要额外转发的，当前就在主进程
        // 如果存在 pending request，由调用方自行处理
        console.warn(`[IpcRouter] 消息路由到 MAIN（直接消费）: channel=${envelope.channel}`);
        break;

      case ProcessId.RENDERER:
        this.deliverToRenderer(envelope);
        break;

      case ProcessId.LLM_SERVICE:
      case ProcessId.PLUGIN_HOST:
        this.deliverToChildProcess(target, envelope);
        break;

      default:
        throw new Error(`[IpcRouter] 未知的目标进程: ${target}`);
    }
  }

  /**
   * 投递消息到 Renderer 进程
   */
  private deliverToRenderer(envelope: IpcEnvelope): void {
    const win = this.mainWindow;
    if (!win || win.isDestroyed()) {
      throw new Error('[IpcRouter] 主窗口不存在或已销毁，无法发送消息到 Renderer');
    }
    win.webContents.send('ipc-message', envelope);
  }

  /**
   * 投递消息到子进程（LLM Service / Plugin Host）
   */
  private deliverToChildProcess(processId: ProcessId, envelope: IpcEnvelope): void {
    const sendFn = this.childProcessSenders.get(processId);
    if (!sendFn) {
      throw new Error(`[IpcRouter] 子进程未注册 send 函数: ${processId}`);
    }
    sendFn(envelope);
  }

  /**
   * 校验信封格式
   */
  private validateEnvelope(envelope: unknown, sourceLabel: string): envelope is IpcEnvelope {
    if (!envelope || typeof envelope !== 'object') {
      console.error(`[IpcRouter] 来自 ${sourceLabel} 的消息不是有效对象`);
      return false;
    }

    const e = envelope as Record<string, unknown>;
    if (typeof e.id !== 'string' || !e.id) {
      console.error(`[IpcRouter] 来自 ${sourceLabel} 的消息缺少有效 id`);
      return false;
    }

    if (typeof e.channel !== 'string' || !Object.values(IpcChannel).includes(e.channel as IpcChannel)) {
      console.error(`[IpcRouter] 来自 ${sourceLabel} 的消息包含无效 channel: ${e.channel}`);
      return false;
    }

    return true;
  }

  /**
   * 清理所有 pending request（应用退出时调用）
   */
  dispose(): void {
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

/** 全局单例 */
export const ipcRouter = new IpcRouter();
