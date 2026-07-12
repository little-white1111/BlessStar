/**
 * 组件定义辅助函数
 * 用于将 React 组件包装为 Web Components，或定义原生 Web Components
 * 遵循架构不变量 I3：组件必须通过 Web Components 标准接口加载
 * 遵循架构不变量 I8：SDK 零依赖 — 仅 TypeScript + Web API
 */

import type { DefineComponentOptions } from '../types/component';

/**
 * 定义一个 Web Components 组件
 * 自动处理生命周期、属性变更、渲染调度
 *
 * @example
 * ```ts
 * const { register } = defineComponent({
 *   tagName: 'danmaku-list',
 *   config: {
 *     name: '弹幕列表',
 *     version: '1.0.0',
 *     propsSchema: {
 *       fontSize: { type: 'number', label: '字体大小', default: 14 },
 *     },
 *   },
 *   render: (props) => `<div>...</div>`,
 * });
 * register();
 * ```
 */
export function defineComponent(options: DefineComponentOptions) {
  const { tagName, config, render, hooks } = options;

  // 验证 tagName 是否符合 Web Components 命名规范（必须包含连字符）
  if (!tagName.includes('-')) {
    throw new Error(
      `[PluginSDK] Component tagName "${tagName}" must contain a hyphen (e.g., "danmaku-list")`,
    );
  }

  // 验证 tagName 是否已注册
  if (customElements.get(tagName)) {
    console.warn(`[PluginSDK] Component "${tagName}" already registered, skipping`);
    return {
      tagName,
      registration: { tagName, name: config.name, version: config.version, success: false, error: 'Already registered' },
      register: () => false,
    };
  }

  /**
   * 组件基类（继承 HTMLElement）
   * 实现标准的 Web Components 生命周期
   */
  class LivestyleComponent extends HTMLElement {
    _props: Record<string, unknown> = { ...config.defaultProps };
    _container: HTMLElement | null = null;

    constructor() {
      super();
      this.attachShadow({ mode: 'open' });
    }

    /** Web Components 标准生命周期：元素被插入 DOM */
    connectedCallback(): void {
      this._container = document.createElement('div');
      this._container.style.cssText = 'width:100%;height:100%;overflow:hidden;';
      this.shadowRoot!.appendChild(this._container);
      this.renderContent();
      hooks?.onMount?.();
    }

    /** Web Components 标准生命周期：元素从 DOM 移除 */
    disconnectedCallback(): void {
      hooks?.onUnmount?.();
      this._container = null;
    }

    /** Web Components 标准生命周期：监听属性变更 */
    static get observedAttributes(): string[] {
      return Object.keys(config.propsSchema).map((k) =>
        k.replace(/([A-Z])/g, '-$1').toLowerCase(),
      );
    }

    /** Web Components 标准生命周期：属性变更回调 */
    attributeChangedCallback(name: string, oldValue: string | null, newValue: string | null): void {
      if (oldValue === newValue) return;

      const propName = this.toCamelCase(name);
      const oldProps = { ...this._props };
      this._props[propName] = this.parseAttribute(propName, newValue);
      this.renderContent();
      hooks?.onPropsUpdate?.(oldProps, this._props);
    }

    /** 渲染内容 */
    renderContent(): void {
      if (!this._container) return;
      const content = render(this._props);
      if (typeof content === 'string') {
        this._container.innerHTML = content;
      } else if (content instanceof HTMLElement) {
        this._container.innerHTML = '';
        this._container.appendChild(content);
      }
    }

    /** 解析属性值为对应类型 */
    parseAttribute(propName: string, value: string | null): unknown {
      if (value === null) return undefined;

      const definition = config.propsSchema[propName];
      if (!definition) return value;

      switch (definition.type) {
        case 'number':
        case 'slider':
          return Number(value);
        case 'boolean':
          return value === '' || value === 'true';
        default:
          return value;
      }
    }

    /** 烤肉串转驼峰 */
    toCamelCase(str: string): string {
      return str.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
    }
  }

  /**
   * 注册组件到 CustomElementRegistry
   */
  function register(): boolean {
    if (customElements.get(tagName)) {
      return false;
    }
    customElements.define(tagName, LivestyleComponent);
    return true;
  }

  return {
    tagName,
    componentClass: LivestyleComponent,
    registration: {
      tagName,
      name: config.name,
      version: config.version,
      success: true,
    },
    register,
  };
}
