/**
 * PostMessage 传输适配器
 * 用于 iframe parent ←→ child 之间的通信
 * 主方案：浏览器环境下通过 postMessage + MessageChannel 实现双向通信
 */

import type { TransportAdapter, TransportStatus, TransportConfig } from './TransportAdapter';

interface PostMessageTransportOptions extends TransportConfig {
  targetWindow: Window;
  targetOrigin: string;
}

export class PostMessageTransport implements TransportAdapter {
  private targetWindow: Window;
  private targetOrigin: string;
  private messageHandler: ((message: string) => void) | null = null;
  private statusHandler: ((status: TransportStatus) => void) | null = null;
  private _status: TransportStatus = 'disconnected';
  private timeoutMs: number;
  private maxRetries: number;
  private retryIntervalMs: number;
  private retryCount = 0;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;

  constructor(options: PostMessageTransportOptions) {
    this.targetWindow = options.targetWindow;
    this.targetOrigin = options.targetOrigin;
    this.timeoutMs = options.timeoutMs ?? 10000;
    this.maxRetries = options.maxRetries ?? 3;
    this.retryIntervalMs = options.retryIntervalMs ?? 2000;
  }

  get status(): TransportStatus {
    return this._status;
  }

  connect(): void {
    if (this._status === 'connected') return;
    this._status = 'connecting';
    this.emitStatus('connecting');

    window.addEventListener('message', this.handleIncomingMessage);

    // 连接超时检测
    setTimeout(() => {
      if (this._status === 'connecting') {
        this.handleConnectionError(new Error('连接超时'));
      }
    }, this.timeoutMs);
  }

  send(message: string): void {
    if (this._status !== 'connected') {
      console.warn('[PostMessageTransport] 未连接，消息被丢弃');
      return;
    }
    this.targetWindow.postMessage(message, this.targetOrigin);
  }

  onMessage(handler: (message: string) => void): void {
    this.messageHandler = handler;
  }

  onStatusChange(handler: (status: TransportStatus) => void): void {
    this.statusHandler = handler;
  }

  disconnect(): void {
    this._status = 'disconnected';
    this.emitStatus('disconnected');
    window.removeEventListener('message', this.handleIncomingMessage);
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  /** 确认连接（由对端调用 window.parent.postMessage 触发） */
  confirmConnected(): void {
    this._status = 'connected';
    this.retryCount = 0;
    this.emitStatus('connected');
  }

  // ========== 私有方法 ==========

  private handleIncomingMessage = (event: MessageEvent): void => {
    if (this.targetOrigin !== '*' && event.origin !== this.targetOrigin) return;
    if (typeof event.data !== 'string') return;

    // 连接确认消息
    if (event.data === '__LIVESTYLE_CONNECTED__') {
      this.confirmConnected();
      return;
    }

    this.messageHandler?.(event.data);
  };

  private handleConnectionError(error: Error): void {
    console.warn('[PostMessageTransport] 连接错误:', error.message);
    if (this.retryCount < this.maxRetries) {
      this.retryCount++;
      this._status = 'reconnecting';
      this.emitStatus('reconnecting');
      this.reconnectTimer = setTimeout(() => this.connect(), this.retryIntervalMs);
    } else {
      this._status = 'disconnected';
      this.emitStatus('disconnected');
    }
  }

  private emitStatus(status: TransportStatus): void {
    this.statusHandler?.(status);
  }
}
