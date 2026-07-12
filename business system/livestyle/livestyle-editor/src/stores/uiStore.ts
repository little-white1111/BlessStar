/**
 * UI 状态管理（Zustand）
 * 管理面板显隐/尺寸等 UI 相关状态
 * 遵循架构不变量 I9：图层管理与渲染顺序必须解耦
 */

import { create } from 'zustand';

/** 面板尺寸 */
interface PanelSizes {
  layerPanel: number;  // 宽度 px
  componentPalette: number;
  propsPanel: number;
}

interface UIStore {
  /** 面板显隐 */
  showLayerPanel: boolean;
  showComponentPalette: boolean;
  showPropsPanel: boolean;
  /** 面板尺寸 */
  panelSizes: PanelSizes;

  toggleLayerPanel: () => void;
  toggleComponentPalette: () => void;
  togglePropsPanel: () => void;
  setPanelSize: (panel: keyof PanelSizes, size: number) => void;
  /** 重置面板布局 */
  resetLayout: () => void;
}

const DEFAULT_PANEL_SIZES: PanelSizes = {
  layerPanel: 240,
  componentPalette: 240,
  propsPanel: 300,
};

export const useUIStore = create<UIStore>((set) => ({
  showLayerPanel: true,
  showComponentPalette: true,
  showPropsPanel: true,
  panelSizes: { ...DEFAULT_PANEL_SIZES },

  toggleLayerPanel: () => set((s) => ({ showLayerPanel: !s.showLayerPanel })),
  toggleComponentPalette: () => set((s) => ({ showComponentPalette: !s.showComponentPalette })),
  togglePropsPanel: () => set((s) => ({ showPropsPanel: !s.showPropsPanel })),

  setPanelSize: (panel, size) => {
    set((s) => ({
      panelSizes: { ...s.panelSizes, [panel]: Math.max(180, Math.min(500, size)) },
    }));
  },

  resetLayout: () => {
    set({
      showLayerPanel: true,
      showComponentPalette: true,
      showPropsPanel: true,
      panelSizes: { ...DEFAULT_PANEL_SIZES },
    });
  },
}));
