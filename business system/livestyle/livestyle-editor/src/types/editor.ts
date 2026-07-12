/** 编辑器全局状态 */
export interface EditorState {
  /** 当前选中的组件 ID */
  selectedComponentId: string | null;
  /** 缩放级别 */
  zoom: number;
  /** 画布偏移 */
  panOffset: { x: number; y: number };
  /** 是否显示网格 */
  showGrid: boolean;
  /** 撤销栈 */
  undoStack: EditorAction[];
  /** 重做栈 */
  redoStack: EditorAction[];
  /** 编辑器模式 */
  mode: EditorMode;
}

/** 编辑器操作模式 */
export enum EditorMode {
  SELECT = 'select',
  MOVE = 'move',
  RESIZE = 'resize',
  HAND = 'hand',
}

/** 可撤销的编辑器动作 */
export interface EditorAction {
  type: string;
  payload: unknown;
  timestamp: number;
}

/** 默认编辑器状态 */
export const DEFAULT_EDITOR_STATE: EditorState = {
  selectedComponentId: null,
  zoom: 1,
  panOffset: { x: 0, y: 0 },
  showGrid: true,
  undoStack: [],
  redoStack: [],
  mode: EditorMode.SELECT,
};
