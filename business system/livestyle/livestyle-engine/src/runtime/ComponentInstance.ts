/**
 * 组件实例包装器
 * 管理单个 Web Components 实例的生命周期
 * 遵循架构不变量 I3：通过 Web Components 标准接口加载
 * 遵循架构不变量 I11：单向数据流 — 编辑器 → ComponentEngine → 组件实例
 */

import type {
  ComponentId,
  ComponentMeta,
  ComponentTagName,
  IComponentInstance,
  PropsConfig,
} from '../types/component';
import { ComponentState } from '../types/component';

/**
 * 组件实例包装器
 * 封装原生 Web Components 实例，提供生命周期管理和属性 diff 更新
 */
export class ComponentInstance implements IComponentInstance {
  readonly id: ComponentId;
  readonly tagName: ComponentTagName;
  readonly meta: ComponentMeta;
  state: ComponentState = ComponentState.CREATED;
  props: PropsConfig;

  private element: HTMLElement | null = null;
  private container: HTMLElement | null = null;

  constructor(meta: ComponentMeta, initialProps?: PropsConfig) {
    this.id = meta.id;
    this.tagName = meta.tagName;
    this.meta = meta;
    this.props = { ...meta.defaultProps, ...initialProps };
  }

  /**
   * 挂载组件到 DOM 容器
   * 创建一个 Web Components 实例并设置初始属性
   */
  mount(container: HTMLElement): void {
    if (this.state === ComponentState.MOUNTED) {
      console.warn(
        `[ComponentInstance] Component "${this.id}" already mounted`,
      );
      return;
    }

    this.container = container;

    try {
      // 创建 Web Components 实例
      this.element = document.createElement(this.tagName);
      this.applyProps(this.props);

      container.appendChild(this.element);
      this.state = ComponentState.MOUNTED;
    } catch (err) {
      this.state = ComponentState.ERROR;
      throw err;
    }
  }

  /**
   * 更新组件属性
   * 只对变更的属性进行 diff 更新，避免不必要的渲染
   */
  update(newProps: PropsConfig): void {
    if (this.state === ComponentState.UNMOUNTED) {
      console.warn(
        `[ComponentInstance] Cannot update unmounted component "${this.id}"`,
      );
      return;
    }

    const changedProps = this.diffProps(this.props, newProps);

    if (Object.keys(changedProps).length === 0) {
      return; // 无变更
    }

    this.props = { ...this.props, ...newProps };

    if (this.element) {
      this.applyProps(changedProps);
    }

    this.state = ComponentState.UPDATED;
  }

  /**
   * 卸载组件
   * 从 DOM 中移除并清理引用
   */
  unmount(): void {
    if (this.state === ComponentState.UNMOUNTED) return;

    if (this.element && this.container) {
      this.container.removeChild(this.element);
    }

    this.element = null;
    this.container = null;
    this.state = ComponentState.UNMOUNTED;
  }

  /**
   * 获取 DOM 元素引用
   */
  getDOMElement(): HTMLElement | null {
    return this.element;
  }

  /**
   * 计算属性差异
   * 返回实际发生变更的属性子集
   */
  private diffProps(
    oldProps: PropsConfig,
    newProps: PropsConfig,
  ): PropsConfig {
    const changed: PropsConfig = {};

    for (const key of Object.keys(newProps)) {
      if (oldProps[key] !== newProps[key]) {
        changed[key] = newProps[key];
      }
    }

    return changed;
  }

  /**
   * 将属性应用到 DOM 元素
   * Web Components 通过 attributes 和 properties 接收数据
   */
  private applyProps(props: PropsConfig): void {
    if (!this.element) return;

    for (const [key, value] of Object.entries(props)) {
      const attrName = this.toKebabCase(key);

      if (value === null || value === undefined) {
        this.element.removeAttribute(attrName);
        continue;
      }

      if (typeof value === 'boolean') {
        if (value) {
          this.element.setAttribute(attrName, '');
        } else {
          this.element.removeAttribute(attrName);
        }
        continue;
      }

      if (typeof value === 'object') {
        // 复杂类型通过 property 设置
        (this.element as any)[key] = value;
        // 同时以 JSON 字符串形式设置 attribute
        this.element.setAttribute(attrName, JSON.stringify(value));
      } else {
        this.element.setAttribute(attrName, String(value));
      }
    }
  }

  /**
   * 驼峰转烤肉串格式
   * backgroundColor → background-color
   */
  private toKebabCase(str: string): string {
    return str.replace(/([A-Z])/g, '-$1').toLowerCase();
  }
}
