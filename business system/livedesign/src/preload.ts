/**
 * Preload 脚本 — 暴露安全的 electronAPI 到 Renderer 进程
 * Renderer 进程通过 window.electronAPI 与主进程通信
 * 架构不变量 #1: 所有消息经由 IPC Router
 */
import { contextBridge, ipcRenderer } from 'electron';
import { IpcChannel, IpcEnvelope } from './shared/ipc-protocol';
import { v4 as uuidv4 } from 'uuid';

export type ElectronApiCallback = (...args: unknown[]) => void;

/** Renderer 可用的 API */
const electronAPI = {
  /** 发送消息到主进程（经由 IPC Router），返回 Promise */
  send(channel: IpcChannel, payload: unknown): Promise<unknown> {
    return new Promise((resolve, reject) => {
      const id = uuidv4();
      const envelope: IpcEnvelope = { id, channel, payload };

      const responseChannel = `${channel}:response`;
      const handler = (_event: Electron.IpcRendererEvent, response: IpcEnvelope) => {
        if (response.id === id) {
          ipcRenderer.removeListener(responseChannel, handler);
          if (response.error) {
            reject(new Error(response.error));
          } else {
            resolve(response.payload);
          }
        }
      };

      ipcRenderer.on(responseChannel, handler);
      setTimeout(() => {
        ipcRenderer.removeListener(responseChannel, handler);
        reject(new Error(`IPC request timeout: ${channel}`));
      }, 30000);

      ipcRenderer.send('ipc:message', envelope);
    });
  },

  /** 监听来自主进程的事件 */
  on(channel: string, callback: ElectronApiCallback): void {
    ipcRenderer.on(channel, (_event, ...args) => callback(...args));
  },

  /** 移除监听器 */
  removeListener(channel: string, callback: ElectronApiCallback): void {
    ipcRenderer.removeListener(channel, callback);
  },

  /** 保存悬浮球位置（架构不变量 #10） */
  savePosition(x: number, y: number): Promise<void> {
    return this.send(IpcChannel.CONFIG_CHANGED, {
      config: { key: 'avatar.position_x', value: x },
    }) as Promise<void>;
  },

  /** 获取配置值 */
  getConfig<T>(key: string): Promise<T | undefined> {
    return this.send('config:get' as IpcChannel, { key }) as Promise<T | undefined>;
  },

  /** 设置配置值 */
  setConfig(key: string, value: unknown): Promise<void> {
    return this.send(IpcChannel.CONFIG_CHANGED, { key, value }) as Promise<void>;
  },

  /** 获取可用角色卡列表 */
  getCharacters(): Promise<Array<{ id: string; name: string }>> {
    return this.send('character:list' as IpcChannel, {}) as Promise<Array<{ id: string; name: string }>>;
  },

  /** 切换角色（架构不变量 #12） */
  switchCharacter(roleId: string): Promise<void> {
    return this.send(IpcChannel.LLM_SWITCH_CHARACTER, { roleId }) as Promise<void>;
  },

  /** 发送聊天消息 */
  sendChatMessage(message: string, rolePreset: string, temperature?: number): Promise<void> {
    return this.send(IpcChannel.LLM_CHAT, { message, rolePreset, temperature }) as Promise<void>;
  },

  /** 中止 LLM 生成 */
  abortChat(): Promise<void> {
    return this.send(IpcChannel.LLM_ABORT, {}) as Promise<void>;
  },
};

// 暴露到 Renderer 进程的 window.electronAPI
contextBridge.exposeInMainWorld('electronAPI', electronAPI);
