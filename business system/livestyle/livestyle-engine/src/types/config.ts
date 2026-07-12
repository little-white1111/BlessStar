/**
 * 引擎配置类型定义
 * Phase 1 使用本地 JSON 直读，Phase 2 替换为 BlessStar 驱动
 */

/** 引擎全局配置 */
export interface EngineConfig {
  render: {
    fpsLimit: number;
    dirtyRectEnabled: boolean;
  };
  component: {
    defaultWidth: number;
    defaultHeight: number;
  };
  configDir: string;
}

/** 组件配置文件结构 */
export interface ComponentConfigFile {
  version: string;
  componentId: string;
  tagName: string;
  props: Record<string, unknown>;
  layer?: {
    zIndex: number;
    visible: boolean;
    locked: boolean;
    x: number;
    y: number;
    width: number;
    height: number;
  };
}

/** 配置变更事件 */
export interface ConfigChangeEvent {
  componentId: string;
  path: string;
  oldValue: unknown;
  newValue: unknown;
  timestamp: number;
}
