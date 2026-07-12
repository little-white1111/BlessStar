/**
 * IPC 桥接协议类型定义
 * 用于 editor ↔ engine 之间的通信
 * 遵循架构不变量 I11：单向数据流 — 编辑器 → ComponentEngine → 组件实例
 */

import type { ComponentId, PropsConfig } from './component';

/** 消息方向 */
export enum MessageDirection {
  EDITOR_TO_ENGINE = 'editor→engine',
  ENGINE_TO_EDITOR = 'engine→editor',
}

/** 消息类型枚举 — Phase 2 扩展 */
export enum BridgeMessageType {
  // ======== editor → engine (基础) ========
  SET_COMPONENT_CONFIG = 'setComponentConfig',
  GET_COMPONENT_CONFIG = 'getComponentConfig',
  GET_COMPONENT_STATE = 'getComponentState',
  SET_LAYER_ORDER = 'setLayerOrder',
  NOTIFY_CONFIG_CHANGE = 'notifyConfigChange',
  LOAD_COMPONENT = 'loadComponent',
  UNLOAD_COMPONENT = 'unloadComponent',

  // ======== editor → engine (Phase 2 新增) ========
  /** 组件画板：加载组件 */
  CANVAS_LOAD_COMPONENT = 'canvasLoadComponent',
  /** 组件画板：切换编辑/预览模式 */
  CANVAS_SWITCH_MODE = 'canvasSwitchMode',
  /** 组件画板：推送属性更新 */
  CANVAS_UPDATE_PROPS = 'canvasUpdateProps',
  /** 健康检查 */
  PING = 'ping',
  /** 获取引擎版本 */
  GET_ENGINE_VERSION = 'getEngineVersion',
  /** 配置验证 */
  VALIDATE_CONFIG = 'validateConfig',

  // ======== engine → editor (基础) ========
  COMPONENT_STATE_CHANGED = 'componentStateChanged',
  COMPONENT_ERROR = 'componentError',
  ENGINE_READY = 'engineReady',
  CONFIG_VALIDATED = 'configValidated',

  // ======== engine → editor (Phase 2 新增) ========
  /** 引擎版本响应 */
  ENGINE_VERSION = 'engineVersion',
  /** 健康检查响应 */
  PONG = 'pong',
  /** 组件画板状态推送 */
  CANVAS_STATE_CHANGED = 'canvasStateChanged',
}

/** 桥接消息载荷映射 */
export interface BridgePayloadMap {
  // editor → engine
  [BridgeMessageType.SET_COMPONENT_CONFIG]: {
    componentId: ComponentId;
    props: PropsConfig;
  };
  [BridgeMessageType.GET_COMPONENT_CONFIG]: {
    componentId: ComponentId;
  };
  [BridgeMessageType.GET_COMPONENT_STATE]: {
    componentId: ComponentId;
  };
  [BridgeMessageType.SET_LAYER_ORDER]: {
    layerOrder: LayerOrderItem[];
  };
  [BridgeMessageType.NOTIFY_CONFIG_CHANGE]: {
    componentId: ComponentId;
    path: string;
    newValue: unknown;
  };
  [BridgeMessageType.LOAD_COMPONENT]: {
    componentId: ComponentId;
    tagName: string;
    jsUrl: string;
  };
  [BridgeMessageType.UNLOAD_COMPONENT]: {
    componentId: ComponentId;
  };
  // Phase 2 新增
  [BridgeMessageType.CANVAS_LOAD_COMPONENT]: {
    componentId: ComponentId;
    tagName: string;
    schema: Record<string, unknown>;
    currentProps: Record<string, unknown>;
  };
  [BridgeMessageType.CANVAS_SWITCH_MODE]: {
    mode: 'edit' | 'preview';
  };
  [BridgeMessageType.CANVAS_UPDATE_PROPS]: {
    props: Record<string, unknown>;
  };
  [BridgeMessageType.PING]: Record<string, never>;
  [BridgeMessageType.GET_ENGINE_VERSION]: Record<string, never>;
  [BridgeMessageType.VALIDATE_CONFIG]: {
    schema: Record<string, unknown>;
    values: Record<string, unknown>;
  };

  // engine → editor
  [BridgeMessageType.COMPONENT_STATE_CHANGED]: {
    componentId: ComponentId;
    state: string;
  };
  [BridgeMessageType.COMPONENT_ERROR]: {
    componentId: ComponentId;
    error: string;
  };
  [BridgeMessageType.ENGINE_READY]: {
    version: string;
  };
  [BridgeMessageType.CONFIG_VALIDATED]: {
    componentId: ComponentId;
    valid: boolean;
    errors?: string[];
  };
  // Phase 2 新增
  [BridgeMessageType.ENGINE_VERSION]: {
    version: string;
  };
  [BridgeMessageType.PONG]: {
    timestamp: number;
  };
  [BridgeMessageType.CANVAS_STATE_CHANGED]: {
    mode: 'edit' | 'preview';
    ready: boolean;
  };
}

/** 桥接消息 */
export interface BridgeMessage<T extends BridgeMessageType = BridgeMessageType> {
  id: string;
  type: T;
  direction: MessageDirection;
  payload: BridgePayloadMap[T];
  timestamp: number;
}

/** 图层排序项 */
export interface LayerOrderItem {
  componentId: ComponentId;
  zIndex: number;
  visible: boolean;
  locked: boolean;
}

/** 桥接响应 */
export interface BridgeResponse<T = unknown> {
  messageId: string;
  success: boolean;
  data?: T;
  error?: string;
}
