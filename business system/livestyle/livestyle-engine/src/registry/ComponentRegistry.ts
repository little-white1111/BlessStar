/**
 * 组件注册表
 * 管理 Web Components 的注册、查找、加载
 * 遵循架构不变量 I3：组件必须通过 Web Components 标准接口加载
 * 遵循架构不变量 I7：禁止组件间直接耦合通信
 */

import type {
  ComponentId,
  ComponentMeta,
  RegistryEntry,
  ComponentLoadConfig,
} from '../types/component';
import { EventBus } from '../event/EventBus';

/** 注册表事件名称 */
export const RegistryEvents = {
  COMPONENT_REGISTERED: 'registry:componentRegistered',
  COMPONENT_UNREGISTERED: 'registry:componentUnregistered',
  COMPONENT_LOADED: 'registry:componentLoaded',
  COMPONENT_LOAD_ERROR: 'registry:componentLoadError',
} as const;

/**
 * 组件注册表
 * 职责：注册、查找、动态加载 Web Components
 */
export class ComponentRegistry {
  private entries = new Map<ComponentTagName, RegistryEntry>();
  private idIndex = new Map<ComponentId, ComponentTagName>();
  private eventBus: EventBus;

  constructor(eventBus: EventBus) {
    this.eventBus = eventBus;
  }

  /**
   * 注册组件
   * 将组件类注册为 customElement
   */
  register(meta: ComponentMeta, componentClass: CustomElementConstructor): void {
    const { tagName } = meta;

    if (this.entries.has(tagName)) {
      throw new Error(
        `[ComponentRegistry] Component "${tagName}" already registered`,
      );
    }

    // 检查是否已通过 customElements.define 注册
    if (!customElements.get(tagName)) {
      customElements.define(tagName, componentClass);
    }

    this.entries.set(tagName, {
      meta,
      componentClass,
      loaded: true,
    });
    this.idIndex.set(meta.id, tagName);

    this.eventBus.emit(RegistryEvents.COMPONENT_REGISTERED, { meta });
  }

  /**
   * 取消注册组件
   */
  unregister(tagName: ComponentTagName): void {
    const entry = this.entries.get(tagName);
    if (!entry) return;

    this.entries.delete(tagName);
    this.idIndex.delete(entry.meta.id);
    this.eventBus.emit(RegistryEvents.COMPONENT_UNREGISTERED, {
      tagName,
      meta: entry.meta,
    });
  }

  /**
   * 动态加载组件 JS
   * 通过创建 <script> 标签加载远程组件
   */
  async loadComponent(
    componentId: ComponentId,
    config: ComponentLoadConfig,
  ): Promise<void> {
    const tagName = config.meta?.tagName || componentId;

    if (this.entries.has(tagName)) {
      return; // 已加载
    }

    try {
      await this.loadScript(config.jsUrl);

      if (config.cssUrl) {
        await this.loadStylesheet(config.cssUrl);
      }

      this.eventBus.emit(RegistryEvents.COMPONENT_LOADED, {
        componentId,
        tagName,
      });
    } catch (err) {
      const error =
        err instanceof Error ? err : new Error(`Failed to load component: ${tagName}`);
      this.entries.set(tagName, {
        meta: {
          id: componentId,
          tagName,
          name: tagName,
          version: '0.0.0',
          defaultProps: {},
        },
        componentClass: class Empty extends HTMLElement {},
        loaded: false,
        loadingError: error,
      });
      this.eventBus.emit(RegistryEvents.COMPONENT_LOAD_ERROR, {
        componentId,
        tagName,
        error: error.message,
      });
      throw error;
    }
  }

  /**
   * 获取注册条目
   */
  getEntry(tagName: ComponentTagName): RegistryEntry | undefined {
    return this.entries.get(tagName);
  }

  /**
   * 通过组件 ID 获取注册条目
   */
  getEntryById(componentId: ComponentId): RegistryEntry | undefined {
    const tagName = this.idIndex.get(componentId);
    if (!tagName) return undefined;
    return this.entries.get(tagName);
  }

  /**
   * 获取所有已注册组件元数据
   */
  getAllMeta(): ComponentMeta[] {
    return Array.from(this.entries.values()).map((e) => e.meta);
  }

  /**
   * 检查组件是否已注册
   */
  isRegistered(tagName: ComponentTagName): boolean {
    return this.entries.has(tagName);
  }

  /**
   * 获取注册数量
   */
  get size(): number {
    return this.entries.size;
  }

  /**
   * 清除所有注册
   */
  clear(): void {
    this.entries.clear();
    this.idIndex.clear();
  }

  private loadScript(url: string): Promise<void> {
    return new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = url;
      script.async = true;
      script.onload = () => resolve();
      script.onerror = () =>
        reject(new Error(`Failed to load script: ${url}`));
      document.head.appendChild(script);
    });
  }

  private loadStylesheet(url: string): Promise<void> {
    return new Promise((resolve, reject) => {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = url;
      link.onload = () => resolve();
      link.onerror = () =>
        reject(new Error(`Failed to load stylesheet: ${url}`));
      document.head.appendChild(link);
    });
  }
}

/** 组件标签名 */
type ComponentTagName = string;
