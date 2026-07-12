/**
 * 组件注册与属性管理 Store（Zustand）
 * 管理组件注册信息、Schema 和属性缓存
 * 遵循架构不变量 I8：组件 SDK 零依赖，Store 仅管理元数据
 * 遵循架构不变量 I11：单向数据流 — 编辑器 → ComponentEngine → 组件实例
 */

import { create } from 'zustand';
import { generateId } from '../utils/idGenerator';
import type { CanvasComponent } from '../types/component';

/** 组件 Schema 描述 — 供属性面板渲染使用 */
export interface ComponentSchemaField {
  key: string;
  label: string;
  type: 'string' | 'number' | 'boolean' | 'color' | 'select' | 'textarea';
  default: unknown;
  options?: { label: string; value: string }[];  // select 类型专用
  min?: number;  // number 类型专用
  max?: number;
  description?: string;
}

export interface ComponentSchema {
  tagName: string;
  name: string;
  fields: ComponentSchemaField[];
}

interface ComponentStore {
  /** 画布上的组件实例 */
  components: CanvasComponent[];

  /** 组件 Schema 注册表（组件面板 + 属性面板使用） */
  schemas: Map<string, ComponentSchema>; // tagName → Schema

  /** 选中状态 */
  selectedComponentId: string | null;

  /** 注册组件 Schema */
  registerSchema: (schema: ComponentSchema) => void;
  /** 获取组件 Schema */
  getSchema: (tagName: string) => ComponentSchema | undefined;

  /** 添加组件到画布 */
  addComponent: (tagName: string, name: string) => CanvasComponent;
  /** 移除组件 */
  removeComponent: (id: string) => void;
  /** 更新组件属性 */
  updateComponent: (id: string, partial: Partial<CanvasComponent>) => void;
  /** 更新组件的 props 字段 */
  updateComponentProps: (id: string, props: Record<string, unknown>) => void;
  /** 批量设置 */
  setComponents: (components: CanvasComponent[]) => void;

  /** 选中 */
  selectComponent: (id: string | null) => void;
  /** 获取选中的组件 */
  getSelectedComponent: () => CanvasComponent | undefined;
}

export const useComponentStore = create<ComponentStore>((set, get) => ({
  components: [],
  schemas: new Map(),
  selectedComponentId: null,

  registerSchema: (schema) => {
    set((state) => {
      const newSchemas = new Map(state.schemas);
      newSchemas.set(schema.tagName, schema);
      return { schemas: newSchemas };
    });
  },

  getSchema: (tagName) => {
    return get().schemas.get(tagName);
  },

  addComponent: (tagName, name) => {
    const newComponent: CanvasComponent = {
      id: generateId(),
      tagName,
      name,
      x: 100,
      y: 100,
      width: 400,
      height: 300,
      rotation: 0,
      zIndex: get().components.length,
      visible: true,
      locked: false,
      props: {},
    };
    set((state) => ({
      components: [...state.components, newComponent],
    }));
    return newComponent;
  },

  removeComponent: (id) => {
    set((state) => ({
      components: state.components.filter((c) => c.id !== id),
      selectedComponentId:
        state.selectedComponentId === id ? null : state.selectedComponentId,
    }));
  },

  updateComponent: (id, partial) => {
    set((state) => ({
      components: state.components.map((c) =>
        c.id === id ? { ...c, ...partial } : c,
      ),
    }));
  },

  updateComponentProps: (id, props) => {
    set((state) => ({
      components: state.components.map((c) =>
        c.id === id ? { ...c, props: { ...c.props, ...props } } : c,
      ),
    }));
  },

  setComponents: (components) => set({ components }),

  selectComponent: (id) => set({ selectedComponentId: id }),

  getSelectedComponent: () => {
    const { components, selectedComponentId } = get();
    return components.find((c) => c.id === selectedComponentId);
  },
}));
