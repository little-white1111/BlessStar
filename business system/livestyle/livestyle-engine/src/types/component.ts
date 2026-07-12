/**
 * 组件运行时类型定义
 * 遵循架构不变量 I3：组件必须通过 Web Components 标准接口加载
 */

/** 组件唯一标识 */
export type ComponentId = string;

/** 组件名称（用于注册 customElements） */
export type ComponentTagName = string;

/** 组件属性键值对 */
export interface PropsConfig {
  [key: string]: unknown;
}

/** 组件元数据 */
export interface ComponentMeta {
  id: ComponentId;
  tagName: ComponentTagName;
  name: string;
  description?: string;
  version: string;
  author?: string;
  thumbnail?: string;
  defaultProps: PropsConfig;
}

/** 组件实例状态 */
export enum ComponentState {
  CREATED = 'created',
  MOUNTED = 'mounted',
  UPDATED = 'updated',
  ERROR = 'error',
  UNMOUNTED = 'unmounted',
}

/** 组件实例运行时接口 */
export interface IComponentInstance {
  readonly id: ComponentId;
  readonly tagName: ComponentTagName;
  readonly meta: ComponentMeta;
  state: ComponentState;
  props: PropsConfig;

  mount(container: HTMLElement): void;
  update(newProps: PropsConfig): void;
  unmount(): void;
  getDOMElement(): HTMLElement | null;
}

/** 组件注册表条目 */
export interface RegistryEntry {
  meta: ComponentMeta;
  componentClass: CustomElementConstructor;
  loaded: boolean;
  loadingError?: Error;
}

/** 组件加载配置 */
export interface ComponentLoadConfig {
  jsUrl: string;
  cssUrl?: string;
  meta?: Partial<ComponentMeta>;
}
