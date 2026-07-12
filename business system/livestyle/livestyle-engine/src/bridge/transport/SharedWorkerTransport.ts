/**
 * SharedWorker 传输适配器
 * 用于多页面共享同一引擎实例的场景
 * 通过 SharedWorker 的 port 实现双向通信
 */

import type { TransportAdapter, TransportStatus, TransportConfig } from './TransportAdapter';

interface SharedWorkerTransportOptions extends TransportConfig {
  sharedWorker: SharedWorker;
}

export class SharedWorkerTransport implements TransportAdapter {
  private port: MessagePort;
  private messageHandler: ((message: string) => void) | null = null;
  private statusHandler: ((status: TransportStatus) => void) | null = null;
  private _status: TransportStatus = 'disconnected';
  private timeoutMs: number;
  private maxRetries: number;
  private retryIntervalMs: number;
  private retryCount = 0;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;

  constructor(options: SharedWorkerTransportOptions) {
    this.port = options.sharedWorker.port;
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

    this.port.onmessage = this.handleMessage;
    this.port.onmessageerror = this.handleMessageError;
    this.port.start();

    setTimeout(() => {
      if (this._status === 'connecting') {
        this.handleConnectionError(new Error('SharedWorker 连接超时'));
      }
    }, this.timeoutMs);
  }

  send(message: string): void {
    if (this._status !== 'connected') {
      console.warn('[SharedWorkerTransport] 未连接，消息被丢弃');
      return;
    }
    this.port.postMessage(message);
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
    this.port.close();
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  confirmConnected(): void {
    this._status = 'connected';
    this.retryCount = 0;
    this.emitStatus('connected');
  }

  // ========== 私有方法 ==========

  private handleMessage = (event: MessageEvent): void => {
    if (typeof event.data !== 'string') return;

    if (event.data === '__LIVESTYLE_CONNECTED__') {
      this.confirmConnected();
      return;
    }

    this.messageHandler?.(event.data);
  };

  private handleMessageError = (_error: MessageEvent): void => {
    this.handleConnectionError(new Error('SharedWorker 消息错误'));
  };

  private handleConnectionError(error: Error): void {
    console.warn('[SharedWorkerTransport] 错误:', error.message);
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
