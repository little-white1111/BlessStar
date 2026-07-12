/**
 * 图层状态管理（Zustand）
 * 遵循架构不变量 I9：图层管理与渲染顺序必须解耦
 * 图层 Store 在编辑器侧独立维护，引擎只消费排序结果
 */

import { create } from 'zustand';
import type { LayerItem } from '../types/layer';

interface LayerStore {
  layers: LayerItem[];

  /** 添加图层 */
  addLayer: (componentId: string, name: string) => void;
  /** 移除图层 */
  removeLayer: (id: string) => void;
  /** 拖拽排序 */
  reorderLayers: (fromIndex: number, toIndex: number) => void;
  /** 切换显隐 */
  toggleLayerVisibility: (id: string) => void;
  /** 切换锁定 */
  toggleLayerLock: (id: string) => void;
  /** 批量设置 */
  setLayers: (layers: LayerItem[]) => void;
  /** 更新图层名称 */
  renameLayer: (id: string, name: string) => void;
  /** 获取排序后的图层（供引擎同步使用） */
  getOrderedLayers: () => LayerItem[];
}

export const useLayerStore = create<LayerStore>((set, get) => ({
  layers: [],

  addLayer: (componentId, name) => {
    set((state) => ({
      layers: [
        ...state.layers,
        {
          id: componentId,
          name,
          zIndex: state.layers.length,
          visible: true,
          locked: false,
          type: 'component',
        },
      ],
    }));
  },

  removeLayer: (id) => {
    set((state) => ({
      layers: state.layers.filter((l) => l.id !== id),
    }));
  },

  reorderLayers: (fromIndex, toIndex) => {
    set((state) => {
      const newLayers = [...state.layers];
      const [moved] = newLayers.splice(fromIndex, 1);
      if (!moved) return state;
      newLayers.splice(toIndex, 0, moved);
      return {
        layers: newLayers.map((l, i) => ({ ...l, zIndex: i })),
      };
    });
  },

  toggleLayerVisibility: (id) => {
    set((state) => ({
      layers: state.layers.map((l) =>
        l.id === id ? { ...l, visible: !l.visible } : l,
      ),
    }));
  },

  toggleLayerLock: (id) => {
    set((state) => ({
      layers: state.layers.map((l) =>
        l.id === id ? { ...l, locked: !l.locked } : l,
      ),
    }));
  },

  setLayers: (layers) => set({ layers }),

  renameLayer: (id, name) => {
    set((state) => ({
      layers: state.layers.map((l) =>
        l.id === id ? { ...l, name } : l,
      ),
    }));
  },

  getOrderedLayers: () => {
    return [...get().layers].sort((a, b) => a.zIndex - b.zIndex);
  },
}));
