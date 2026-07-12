/**
 * livestyle-engine 入口
 * 组件运行时引擎：管理 Web Components 注册、实例化、渲染调度、配置管理
 */

export { EventBus } from './event/EventBus';
export { ComponentRegistry, RegistryEvents } from './registry/ComponentRegistry';
export { ComponentInstance } from './runtime/ComponentInstance';
export { BridgeProtocol } from './bridge/BridgeProtocol';
export { EditorBridge, BridgeEvents } from './bridge/EditorBridge';
export type { TransportAdapter } from './bridge/EditorBridge';

// 类型导出
export type {
  ComponentId,
  ComponentMeta,
  ComponentTagName,
  PropsConfig,
  IComponentInstance,
  RegistryEntry,
  ComponentLoadConfig,
} from './types/component';
export { ComponentState } from './types/component';

export type {
  EngineConfig,
  ComponentConfigFile,
  ConfigChangeEvent,
} from './types/config';

export type {
  BridgeMessage,
  BridgeResponse,
  LayerOrderItem,
  BridgePayloadMap,
} from './types/protocol';
export {
  BridgeMessageType,
  MessageDirection,
} from './types/protocol';
