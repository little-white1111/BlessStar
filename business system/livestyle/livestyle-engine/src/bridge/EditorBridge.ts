/**
 * 编辑器桥接层
 * 负责 editor ↔ engine 之间的 IPC 通信
 * 遵循架构不变量 I11：单向数据流 — 编辑器 → ComponentEngine → 组件实例
 */

import { BridgeProtocol } from './BridgeProtocol';
import { EventBus } from '../event/EventBus';
import {
  BridgeMessage,
  BridgeMessageType,
  MessageDirection,
} from '../types/protocol';
import type { ComponentId, PropsConfig } from '../types/component';
import type { LayerOrderItem, BridgeResponse } from '../types/protocol';

/** 传输适配器接口 — 支持多种通信方式 */
export interface TransportAdapter {
  send(message: string): void;
  onMessage(handler: (message: string) => void): void;
  disconnect(): void;
}

/** 编辑器桥接事件 */
export const BridgeEvents = {
  MESSAGE_RECEIVED: 'bridge:messageReceived',
  MESSAGE_SENT: 'bridge:messageSent',
  CONNECTION_OPENED: 'bridge:connectionOpened',
  CONNECTION_CLOSED: 'bridge:connectionClosed',
  ERROR: 'bridge:error',
} as const;

type PendingResolver = {
  resolve: (data: unknown) => void;
  reject: (err: Error) => void;
  timer: ReturnType<typeof setTimeout>;
};

/**
 * 编辑器桥接层
 * 包装 TransportAdapter，提供类型安全的通信接口
 */
export class EditorBridge {
  private transport: TransportAdapter;
  private eventBus: EventBus;
  private connected = false;
  private pendingResponses = new Map<string, PendingResolver>();

  /** 默认超时时间 10s */
  private readonly timeoutMs = 10000;

  constructor(transport: TransportAdapter, eventBus: EventBus) {
    this.transport = transport;
    this.eventBus = eventBus;

    this.transport.onMessage((raw: string) => {
      const message = BridgeProtocol.deserialize(raw);
      if (!message) return;

      this.eventBus.emit(BridgeEvents.MESSAGE_RECEIVED, message);
      this.handleMessage(message);
    });
  }

  connect(): void {
    if (this.connected) return;
    this.connected = true;
    this.eventBus.emit(BridgeEvents.CONNECTION_OPENED, {});
  }

  disconnect(): void {
    this.connected = false;
    this.transport.disconnect();
    this.eventBus.emit(BridgeEvents.CONNECTION_CLOSED, {});

    for (const [, pending] of this.pendingResponses) {
      clearTimeout(pending.timer);
      pending.reject(new Error('Bridge disconnected'));
    }
    this.pendingResponses.clear();
  }

  // ========== 类型安全的 API ==========

  async setComponentConfig(
    componentId: ComponentId,
    props: PropsConfig,
  ): Promise<BridgeResponse> {
    const msg = BridgeProtocol.setComponentConfig(componentId, props);
    return this.sendAndWait(msg);
  }

  async getComponentConfig(
    componentId: ComponentId,
  ): Promise<BridgeResponse> {
    const msg = BridgeProtocol.getComponentConfig(componentId);
    return this.sendAndWait(msg);
  }

  async setLayerOrder(layerOrder: LayerOrderItem[]): Promise<BridgeResponse> {
    const msg = BridgeProtocol.setLayerOrder(layerOrder);
    return this.sendAndWait(msg);
  }

  async loadComponent(
    componentId: ComponentId,
    tagName: string,
    jsUrl: string,
  ): Promise<BridgeResponse> {
    const msg = BridgeProtocol.loadComponent(componentId, tagName, jsUrl);
    return this.sendAndWait(msg);
  }

  async notifyConfigChange(
    componentId: ComponentId,
    path: string,
    newValue: unknown,
  ): Promise<void> {
    const msg = BridgeProtocol.notifyConfigChange(
      componentId,
      path,
      newValue,
    );
    this.send(msg);
  }

  // ========== 内部方法 ==========

  private sendAndWait(msg: BridgeMessage): Promise<BridgeResponse> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingResponses.delete(msg.id);
        reject(new Error(`Bridge timeout: ${msg.type}`));
      }, this.timeoutMs);

      this.pendingResponses.set(msg.id, {
        resolve: resolve as (data: unknown) => void,
        reject,
        timer,
      });
      this.send(msg);
    });
  }

  private send(msg: BridgeMessage): void {
    if (!this.connected) {
      throw new Error('[EditorBridge] Not connected');
    }
    const serialized = BridgeProtocol.serialize(msg);
    this.transport.send(serialized);
    this.eventBus.emit(BridgeEvents.MESSAGE_SENT, msg);
  }

  private handleMessage(message: BridgeMessage): void {
    if (message.direction === MessageDirection.ENGINE_TO_EDITOR) {
      const pending = this.pendingResponses.get(message.id);
      if (pending) {
        clearTimeout(pending.timer);
        pending.resolve(message.payload);
        this.pendingResponses.delete(message.id);
        return;
      }
    }

    switch (message.type) {
      case BridgeMessageType.COMPONENT_STATE_CHANGED:
      case BridgeMessageType.COMPONENT_ERROR:
      case BridgeMessageType.ENGINE_READY:
      case BridgeMessageType.CONFIG_VALIDATED:
        this.eventBus.emit(`bridge:${message.type}`, message.payload);
        break;
    }
  }
}
