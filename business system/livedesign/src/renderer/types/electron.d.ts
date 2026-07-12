/**
 * Preload 脚本暴露给渲染进程的 API 类型声明
 * Renderer 进程必须通过此 API 与主进程通信（禁止直接 import ipcRenderer）
 */
import type { CharacterCard } from '../../shared/character-card';

interface ElectronAPI {
  /** 向主进程发送 IPC 消息 */
  send(channel: string, payload: unknown): void;
  /** 监听主进程转发的 IPC 消息 */
  on(channel: string, callback: (event: unknown, ...args: unknown[]) => void): void;
  /** 移除 IPC 消息监听 */
  removeListener(channel: string, callback: (event: unknown, ...args: unknown[]) => void): void;
  /** 持久化悬浮球位置 */
  savePosition(x: number, y: number): void;
  /** 获取运行时配置 */
  getConfig(): Promise<Record<string, unknown>>;
  /** 更新运行时配置 */
  setConfig(key: string, value: unknown): void;
  /** 获取所有可用角色卡 */
  getCharacters(): Promise<CharacterCard[]>;
  /** 切换当前角色 */
  switchCharacter(card: CharacterCard): void;
}

declare global {
  interface Window {
    electronAPI: ElectronAPI;
  }
}
