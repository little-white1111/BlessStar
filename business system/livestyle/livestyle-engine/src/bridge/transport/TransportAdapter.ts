/**
 * 传输适配器抽象接口
 * 支持多种通信方式：iframe postMessage / Worker / SharedWorker
 * 遵循架构不变量 I14：必须支持连接超时和自动重连
 */
export interface TransportAdapter {
  /** 建立连接 */
  connect(): void;
  /** 发送消息（字符串格式） */
  send(message: string): void;
  /** 注册消息接收处理函数 */
  onMessage(handler: (message: string) => void): void;
  /** 注册连接状态变化回调 */
  onStatusChange(handler: (status: TransportStatus) => void): void;
  /** 断开连接 */
  disconnect(): void;
  /** 当前连接状态 */
  readonly status: TransportStatus;
}

export type TransportStatus = 'disconnected' | 'connecting' | 'connected' | 'reconnecting';

/** 连接配置 */
export interface TransportConfig {
  /** 连接超时时间（ms），默认 10000 */
  timeoutMs?: number;
  /** 最大重试次数，默认 3 */
  maxRetries?: number;
  /** 重试间隔（ms），默认 2000 */
  retryIntervalMs?: number;
}
