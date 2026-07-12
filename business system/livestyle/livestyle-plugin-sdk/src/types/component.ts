/**
 * 组件插件 SDK 公开类型
 * 遵循架构不变量 I8：SDK 零依赖 — 仅 TypeScript + Web API
 */

/** 组件属性描述 */
export interface PropDefinition<T = unknown> {
  type: 'string' | 'number' | 'boolean' | 'color' | 'select' | 'slider';
  label: string;
  default: T;
  options?: { label: string; value: T }[];
  min?: number;
  max?: number;
  step?: number;
  description?: string;
}

/** 组件属性 Schema */
export type PropsSchema = Record<string, PropDefinition>;

/** 组件配置 */
export interface ComponentConfig {
  tagName: string;
  name: string;
  description?: string;
  version: string;
  author?: string;
  thumbnail?: string;
  propsSchema: PropsSchema;
  defaultProps: Record<string, unknown>;
}

/** 组件生命周期钩子 */
export interface ComponentLifecycleHooks {
  onMount?: () => void;
  onUnmount?: () => void;
  onPropsUpdate?: (oldProps: Record<string, unknown>, newProps: Record<string, unknown>) => void;
  onResize?: (width: number, height: number) => void;
}

/** 组件渲染函数 */
export type ComponentRenderFunction = (props: Record<string, unknown>) => string | HTMLElement;

/** 组件定义选项 */
export interface DefineComponentOptions {
  tagName: string;
  config: ComponentConfig;
  render: ComponentRenderFunction;
  hooks?: ComponentLifecycleHooks;
}

/** 组件注册结果 */
export interface ComponentRegistration {
  tagName: string;
  name: string;
  version: string;
  success: boolean;
  error?: string;
}
