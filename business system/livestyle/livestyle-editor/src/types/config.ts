/** 编辑器配置（Phase 1 本地 JSON / Phase 2 BlessStar） */
export interface EditorConfig {
  canvas: {
    width: number;
    height: number;
    gridSize: number;
    snapToGrid: boolean;
    backgroundColor: string;
  };
  autosave: {
    enabled: boolean;
    intervalMs: number;
  };
  editor: {
    maxUndoSteps: number;
    defaultZoom: number;
  };
}

/** 默认编辑器配置 */
export const DEFAULT_EDITOR_CONFIG: EditorConfig = {
  canvas: {
    width: 1920,
    height: 1080,
    gridSize: 20,
    snapToGrid: true,
    backgroundColor: '#1a1a2e',
  },
  autosave: {
    enabled: true,
    intervalMs: 30000,
  },
  editor: {
    maxUndoSteps: 50,
    defaultZoom: 1,
  },
};
