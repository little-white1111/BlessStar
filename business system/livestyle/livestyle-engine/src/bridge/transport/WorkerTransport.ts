/**
 * Worker 传输适配器
 * 用于主线程 ←→ Worker 之间的通信
 * 适用于 Electron 或有 Worker 引擎隔离需求的场景
 */

import type { TransportAdapter, TransportStatus, TransportConfig } from './TransportAdapter';

interface WorkerTransportOptions extends TransportConfig {
  worker: Worker;
}

export class WorkerTransport implements TransportAdapter {
  private worker: Worker;
  private messageHandler: ((message: string) => void) | null = null;
  private statusHandler: ((status: TransportStatus) => void) | null = null;
  private _status: TransportStatus = 'disconnected';
  private timeoutMs: number;
  private maxRetries: number;
  private retryIntervalMs: number;
  private retryCount = 0;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;

  constructor(options: WorkerTransportOptions) {
    this.worker = options.worker;
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

    this.worker.onmessage = this.handleMessage;
    this.worker.onerror = this.handleError;

    setTimeout(() => {
      if (this._status === 'connecting') {
        this.handleConnectionError(new Error('Worker 连接超时'));
      }
    }, this.timeoutMs);
  }

  send(message: string): void {
    if (this._status !== 'connected') {
      console.warn('[WorkerTransport] 未连接，消息被丢弃');
      return;
    }
    this.worker.postMessage(message);
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
    this.worker.terminate();
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

  private handleError = (error: ErrorEvent): void => {
    this.handleConnectionError(new Error(error.message || 'Worker 错误'));
  };

  private handleConnectionError(error: Error): void {
    console.warn('[WorkerTransport] 错误:', error.message);
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
