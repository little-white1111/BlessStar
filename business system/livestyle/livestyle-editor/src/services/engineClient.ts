/**
 * ComponentEngine IPC 客户端
 * 使用 TransportAdapter 与 ComponentEngine 通信
 * 遵循架构不变量 I11：单向数据流 — 编辑器 → ComponentEngine → 组件实例
 * 遵循架构不变量 I13：属性变更推送必须防抖（300ms）
 * 遵循架构不变量 I14：TransportAdapter 必须支持连接超时和自动重连
 */

import type {
  BridgeMessage,
  BridgeResponse,
  LayerOrderItem,
} from 'livestyle-engine/types/protocol';
import {
  BridgeMessageType,
  MessageDirection,
} from 'livestyle-engine/types/protocol';
import { BridgeProtocol } from 'livestyle-engine/bridge/BridgeProtocol';
import type {
  TransportAdapter,
  TransportStatus,
} from 'livestyle-engine/bridge/transport/TransportAdapter';
import { createTransport } from 'livestyle-engine/bridge/transport/TransportFactory';
import type { TransportType } from 'livestyle-engine/bridge/transport/TransportFactory';
import type { ComponentId, PropsConfig } from 'livestyle-engine/types/component';

export type EngineClientStatus = TransportStatus;

export interface EngineClientOptions {
  /** iframe 元素（postMessage 模式） */
  iframe?: HTMLIFrameElement;
  /** Worker 实例（worker 模式） */
  worker?: Worker;
  /** SharedWorker 实例（sharedworker 模式） */
  sharedWorker?: SharedWorker;
  /** 传输类型，默认 'postmessage' */
  transportType?: TransportType;
  /** 连接超时 ms，默认 10000 */
  timeoutMs?: number;
  /** 属性变更防抖 ms，默认 300 */
  debounceMs?: number;
}

/** 防抖工具 */
function debounce<T extends (...args: any[]) => any>(
  fn: T,
  delay: number,
): (...args: Parameters<T>) => void {
  let timer: ReturnType<typeof setTimeout>;
  return (...args: Parameters<T>) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), delay);
  };
}

export class EngineClient {
  private transport: TransportAdapter;
  private statusHandlers: Set<(status: EngineClientStatus) => void> = new Set();
  private messageHandlers: Map<BridgeMessageType, Set<(payload: any) => void>> = new Map();
  private _status: EngineClientStatus = 'disconnected';
  private pendingResponses = new Map<string, {
    resolve: (data: unknown) => void;
    reject: (err: Error) => void;
    timer: ReturnType<typeof setTimeout>;
  }>();
  private readonly timeoutMs: number;
  private readonly debounceMs: number;

  /** 防抖后的属性推送 */
  private debouncedSetConfig: (
    componentId: ComponentId,
    props: PropsConfig,
  ) => void;

  constructor(options: EngineClientOptions = {}) {
    this.timeoutMs = options.timeoutMs ?? 10000;
    this.debounceMs = options.debounceMs ?? 300;

    // 根据 iframe src 推导目标 origin（安全加固：I4 修复 — 替代硬编码 '*'）
    const origin = options.iframe
      ? (() => {
          try {
            return new URL(options.iframe.src).origin;
          } catch {
            return '*'; // 解析失败时降级通配符（dev 模式）
          }
        })()
      : '*';

    this.transport = createTransport({
      type: options.transportType ?? 'postmessage',
      iframeWindow: options.iframe?.contentWindow ?? undefined,
      targetOrigin: origin,
      worker: options.worker,
      sharedWorker: options.sharedWorker,
    });

    this.transport.onMessage(this.handleIncomingMessage);
    this.transport.onStatusChange(this.handleStatusChange);

    // 创建防抖后的属性推送（I13）
    this.debouncedSetConfig = debounce(
      (componentId: ComponentId, props: PropsConfig) => {
        this.transport.send(
          BridgeProtocol.serialize(
            BridgeProtocol.buildEditorMessage(
              BridgeMessageType.SET_COMPONENT_CONFIG,
              { componentId, props },
            ),
          ),
        );
      },
      this.debounceMs,
    );
  }

  get status(): EngineClientStatus {
    return this._status;
  }

  /** 建立连接 */
  connect(): void {
    this.transport.connect();
  }

  /** 断开连接 */
  disconnect(): void {
    this.transport.disconnect();
  }

  /** 注册状态变化回调 */
  onStatusChange(handler: (status: EngineClientStatus) => void): () => void {
    this.statusHandlers.add(handler);
    return () => this.statusHandlers.delete(handler);
  }

  /** 注册消息监听 */
  on<T>(type: BridgeMessageType, handler: (payload: T) => void): () => void {
    if (!this.messageHandlers.has(type)) {
      this.messageHandlers.set(type, new Set());
    }
    this.messageHandlers.get(type)!.add(handler);
    return () => this.messageHandlers.get(type)?.delete(handler);
  }

  // ========== 类型安全的 API ==========

  /** 设置组件配置（防抖推送 — I13） */
  setComponentConfig(componentId: ComponentId, props: PropsConfig): void {
    this.debouncedSetConfig(componentId, props);
  }

  /** 立即设置组件配置（跳过防抖） */
  async setComponentConfigImmediate(
    componentId: ComponentId,
    props: PropsConfig,
  ): Promise<BridgeResponse> {
    // I2 红线：先验证再写入
    const validation = await this.validateConfig(props as Record<string, unknown>);
    if (!validation.success || (validation.data && validation.data.valid === false)) {
      const errors = validation.data?.errors?.join('; ') ?? '未知验证错误';
      console.warn(`[EngineClient] 配置验证未通过，已阻止写入 ${componentId}:`, errors);
      return { messageId: '', success: false, error: `配置验证未通过: ${errors}` };
    }

    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.SET_COMPONENT_CONFIG,
      { componentId, props },
    );
    return this.sendAndWait(msg);
  }

  /** 获取组件配置 */
  async getComponentConfig(componentId: ComponentId): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.GET_COMPONENT_CONFIG,
      { componentId },
    );
    return this.sendAndWait(msg);
  }

  /** 同步图层顺序 */
  async syncLayerOrder(layerOrder: LayerOrderItem[]): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.SET_LAYER_ORDER,
      { layerOrder },
    );
    return this.sendAndWait(msg);
  }

  /** 加载组件 */
  async loadComponent(
    componentId: ComponentId,
    tagName: string,
    jsUrl: string,
  ): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.LOAD_COMPONENT,
      { componentId, tagName, jsUrl },
    );
    return this.sendAndWait(msg);
  }

  /** 卸载组件 */
  async unloadComponent(componentId: ComponentId): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.UNLOAD_COMPONENT,
      { componentId },
    );
    return this.sendAndWait(msg);
  }

  /** 健康检查 */
  async ping(): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.PING,
      {},
    );
    return this.sendAndWait(msg);
  }

  /**
   * 验证配置是否符合 Schema（I2 红线）
   * 在 setComponentConfig 前调用，确保非法配置不会被写入
   */
  async validateConfig(config: Record<string, unknown>): Promise<BridgeResponse & { data?: { valid: boolean; errors?: string[]; warnings?: string[] } }> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.VALIDATE_CONFIG,
      { config },
    );
    return this.sendAndWait(msg) as Promise<any>;
  }

  /** 获取引擎版本 */
  async getEngineVersion(): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.GET_ENGINE_VERSION,
      {},
    );
    return this.sendAndWait(msg) as Promise<BridgeResponse>;
  }

  // ========== 组件画板相关 API ==========

  /** 在组件画板中加载组件 */
  async canvasLoadComponent(
    componentId: ComponentId,
    tagName: string,
    schema: Record<string, unknown>,
    currentProps: Record<string, unknown>,
  ): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.CANVAS_LOAD_COMPONENT,
      { componentId, tagName, schema, currentProps },
    );
    return this.sendAndWait(msg);
  }

  /** 切换组件画板模式 */
  async canvasSwitchMode(mode: 'edit' | 'preview'): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.CANVAS_SWITCH_MODE,
      { mode },
    );
    return this.sendAndWait(msg);
  }

  /** 推送属性到组件画板 */
  async canvasUpdateProps(props: Record<string, unknown>): Promise<BridgeResponse> {
    const msg = BridgeProtocol.buildEditorMessage(
      BridgeMessageType.CANVAS_UPDATE_PROPS,
      { props },
    );
    return this.sendAndWait(msg);
  }

  // ========== 内部方法 ==========

  private handleIncomingMessage = (raw: string): void => {
    const message = BridgeProtocol.deserialize(raw);
    if (!message) return;

    // 处理响应
    if (message.direction === MessageDirection.ENGINE_TO_EDITOR) {
      const pending = this.pendingResponses.get(message.id);
      if (pending) {
        clearTimeout(pending.timer);
        this.pendingResponses.delete(message.id);
        pending.resolve(message.payload);
        return;
      }
    }

    // 分发到注册的消息处理器
    const handlers = this.messageHandlers.get(message.type);
    if (handlers) {
      handlers.forEach((handler) => handler(message.payload));
    }
  };

  private handleStatusChange = (status: TransportStatus): void => {
    this._status = status;
    this.statusHandlers.forEach((handler) => handler(status));
  };

  private sendAndWait(msg: BridgeMessage): Promise<BridgeResponse> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingResponses.delete(msg.id);
        reject(new Error(`EngineClient 超时: ${msg.type} (${this.timeoutMs}ms)`));
      }, this.timeoutMs);

      this.pendingResponses.set(msg.id, {
        resolve: resolve as (data: unknown) => void,
        reject,
        timer,
      });

      this.transport.send(BridgeProtocol.serialize(msg));
    });
  }
}
