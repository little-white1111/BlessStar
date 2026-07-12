/**
 * livestyle-obs-bridge 入口
 * OBS 集成桥接层 — Phase 3 实现
 * 当前为 Phase 1 占位
 */

/** OBS 连接状态 */
export type OBSConnectionState =
  | 'disconnected'
  | 'connecting'
  | 'connected'
  | 'reconnecting';

/** OBS 桥接配置 */
export interface OBSBridgeConfig {
  host: string;
  port: number;
  password?: string;
  autoReconnect: boolean;
  retryIntervalMs: number;
}

/** 默认 OBS 配置 */
export const DEFAULT_OBS_CONFIG: OBSBridgeConfig = {
  host: 'localhost',
  port: 4455,
  password: '',
  autoReconnect: true,
  retryIntervalMs: 5000,
};
