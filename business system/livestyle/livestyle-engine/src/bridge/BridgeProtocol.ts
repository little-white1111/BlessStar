/**
 * IPC 桥接协议定义
 * 定义 editor ↔ engine 之间的通信契约
 */

import {
  BridgeMessage,
  BridgeMessageType,
  BridgeResponse,
  MessageDirection,
} from '../types/protocol';
import type { ComponentId, PropsConfig } from '../types/component';
import type { LayerOrderItem } from '../types/protocol';

let messageIdCounter = 0;

/** 生成唯一消息 ID */
function nextMessageId(): string {
  messageIdCounter++;
  return `msg_${Date.now()}_${messageIdCounter}`;
}

/**
 * 桥接协议实现
 * 提供消息构建、序列化、校验功能
 */
export class BridgeProtocol {
  /**
   * 构建 editor → engine 消息
   */
  static buildEditorMessage<T extends BridgeMessageType>(
    type: T,
    payload: Record<string, unknown>,
  ): BridgeMessage<T> {
    return {
      id: nextMessageId(),
      type,
      direction: MessageDirection.EDITOR_TO_ENGINE,
      payload: payload as any,
      timestamp: Date.now(),
    };
  }

  /**
   * 构建 engine → editor 消息
   */
  static buildEngineMessage<T extends BridgeMessageType>(
    type: T,
    payload: Record<string, unknown>,
  ): BridgeMessage<T> {
    return {
      id: nextMessageId(),
      type,
      direction: MessageDirection.ENGINE_TO_EDITOR,
      payload: payload as any,
      timestamp: Date.now(),
    };
  }

  /**
   * 构建响应消息
   */
  static buildResponse<T = unknown>(
    messageId: string,
    success: boolean,
    data?: T,
    error?: string,
  ): BridgeResponse<T> {
    return { messageId, success, data, error };
  }

  /**
   * 验证消息结构合法性
   */
  static validateMessage(message: unknown): message is BridgeMessage {
    if (!message || typeof message !== 'object') return false;
    const msg = message as Record<string, unknown>;
    return (
      typeof msg.id === 'string' &&
      typeof msg.type === 'string' &&
      typeof msg.direction === 'string' &&
      typeof msg.payload === 'object' &&
      typeof msg.timestamp === 'number'
    );
  }

  /**
   * 序列化消息为字符串
   */
  static serialize(message: BridgeMessage): string {
    return JSON.stringify(message);
  }

  /**
   * 反序列化字符串为消息
   */
  static deserialize(data: string): BridgeMessage | null {
    try {
      const parsed = JSON.parse(data);
      if (BridgeProtocol.validateMessage(parsed)) {
        return parsed;
      }
      return null;
    } catch {
      return null;
    }
  }

  // ========== 快捷构建方法 ==========

  static setComponentConfig(componentId: ComponentId, props: PropsConfig) {
    return BridgeProtocol.buildEditorMessage(
      BridgeMessageType.SET_COMPONENT_CONFIG,
      { componentId, props },
    );
  }

  static getComponentConfig(componentId: ComponentId) {
    return BridgeProtocol.buildEditorMessage(
      BridgeMessageType.GET_COMPONENT_CONFIG,
      { componentId },
    );
  }

  static setLayerOrder(layerOrder: LayerOrderItem[]) {
    return BridgeProtocol.buildEditorMessage(
      BridgeMessageType.SET_LAYER_ORDER,
      { layerOrder },
    );
  }

  static notifyConfigChange(
    componentId: ComponentId,
    path: string,
    newValue: unknown,
  ) {
    return BridgeProtocol.buildEditorMessage(
      BridgeMessageType.NOTIFY_CONFIG_CHANGE,
      { componentId, path, newValue },
    );
  }

  static loadComponent(componentId: ComponentId, tagName: string, jsUrl: string) {
    return BridgeProtocol.buildEditorMessage(
      BridgeMessageType.LOAD_COMPONENT,
      { componentId, tagName, jsUrl },
    );
  }
}
