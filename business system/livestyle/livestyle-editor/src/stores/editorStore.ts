/**
 * 编辑器全局状态管理（Zustand）
 * 遵循架构不变量 I11：单向数据流
 * 已拆分：图层 → layerStore, 组件/属性 → componentStore
 * 本 Store 仅保留：缩放、模式、撤销/重做
 */

import { create } from 'zustand';
import { EditorMode, DEFAULT_EDITOR_STATE } from '../types/editor';
import { useComponentStore } from './componentStore';

interface EditorStore {
  // 编辑器状态
  zoom: number;
  setZoom: (zoom: number) => void;
  mode: EditorMode;
  setMode: (mode: EditorMode) => void;

  // 面板显隐
  showLayerPanel: boolean;
  showComponentPalette: boolean;
  showPropsPanel: boolean;
  toggleLayerPanel: () => void;
  toggleComponentPalette: () => void;
  togglePropsPanel: () => void;

  // 撤销/重做
  undoStack: string[];
  redoStack: string[];
  pushUndo: () => void;
  undo: () => void;
  redo: () => void;
}

export const useEditorStore = create<EditorStore>((set, get) => ({
  // ========== 编辑器状态 ==========
  zoom: DEFAULT_EDITOR_STATE.zoom,
  setZoom: (zoom) => set({ zoom: Math.max(0.1, Math.min(5, zoom)) }),
  mode: EditorMode.SELECT,
  setMode: (mode) => set({ mode }),

  // ========== 面板显隐 ==========
  showLayerPanel: true,
  showComponentPalette: true,
  showPropsPanel: true,
  toggleLayerPanel: () => set((s) => ({ showLayerPanel: !s.showLayerPanel })),
  toggleComponentPalette: () => set((s) => ({ showComponentPalette: !s.showComponentPalette })),
  togglePropsPanel: () => set((s) => ({ showPropsPanel: !s.showPropsPanel })),

  // ========== 撤销/重做 ==========
  undoStack: [],
  redoStack: [],
  pushUndo: () => {
    const components = useComponentStore.getState().components;
    set((state) => ({
      undoStack: [...state.undoStack.slice(-49), JSON.stringify(components)],
      redoStack: [],
    }));
  },
  undo: () => {
    const { undoStack, redoStack } = get();
    if (undoStack.length === 0) return;
    const previous = undoStack[undoStack.length - 1];
    const current = useComponentStore.getState().components;
    useComponentStore.getState().setComponents(JSON.parse(previous));
    set({
      undoStack: undoStack.slice(0, -1),
      redoStack: [...redoStack, JSON.stringify(current)],
    });
  },
  redo: () => {
    const { redoStack, undoStack } = get();
    if (redoStack.length === 0) return;
    const next = redoStack[redoStack.length - 1];
    const current = useComponentStore.getState().components;
    useComponentStore.getState().setComponents(JSON.parse(next));
    set({
      redoStack: redoStack.slice(0, -1),
      undoStack: [...undoStack, JSON.stringify(current)],
    });
  },
}));
