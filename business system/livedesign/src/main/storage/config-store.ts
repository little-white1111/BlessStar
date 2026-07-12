/**
 * 配置存储（架构不变量 #5 — 所有用户数据仅存储在本地）
 * 架构不变量 #10 — 悬浮球位置变更实时持久化
 *
 * 基于 electron-store 实现 ConfigStore 接口，
 * 提供同步的键值对持久化能力。
 */

import ElectronStore from 'electron-store';
import { ConfigStore, ConfigValue } from '../../shared/config-schema';

/** electron-store 的 schema 定义（用于类型约束） */
interface StoreSchema {
  [key: string]: ConfigValue | undefined;
}

class ElectronConfigStore implements ConfigStore {
  private store: ElectronStore<StoreSchema>;

  constructor() {
    this.store = new ElectronStore<StoreSchema>({
      name: 'config', // 存储文件名: config.json
      fileExtension: 'json',
      clearInvalidConfig: true,
      accessPropertiesByDotNotation: true,
      defaults: this.getDefaultConfig(),
    });
  }

  /**
   * 默认配置
   */
  private getDefaultConfig(): StoreSchema {
    return {
      // ============ UI 配置 ============
      'ui.floatBall.x': 0,
      'ui.floatBall.y': 100,
      'ui.floatBall.enabled': true,
      'ui.minimizeToTray': true,
      'ui.language': 'zh-CN',

      // ============ 窗口配置 ============
      'window.x': undefined,
      'window.y': undefined,
      'window.width': 1200,
      'window.height': 800,
      'window.maximized': false,
      'window.alwaysOnTop': false,

      // ============ LLM 配置 ============
      'llm.provider': 'openai',
      'llm.model': 'gpt-4o',
      'llm.temperature': 0.7,
      'llm.maxTokens': 4096,
      'llm.apiEndpoint': 'https://api.openai.com/v1',

      // ============ 当前角色配置 ============
      'character.current': '',

      // ============ 插件配置 ============
      'plugin.autoStart': true,
      'plugin.disabledList': [],

      // ============ 通知配置 ============
      'notification.enabled': true,
      'notification.sound': true,
    };
  }

  /**
   * 获取配置值
   * @param key 配置键（支持点号分隔，如 'ui.floatBall.x'）
   */
  get<T extends ConfigValue>(key: string): T | undefined {
    try {
      return this.store.get(key) as T | undefined;
    } catch (err) {
      console.error(`[ConfigStore] 读取配置失败: key=${key}`, err);
      return undefined;
    }
  }

  /**
   * 设置配置值
   * 架构不变量 #10：悬浮球位置调用此方法时实时持久化到磁盘
   * @param key 配置键
   * @param value 配置值
   */
  set<T extends ConfigValue>(key: string, value: T): void {
    try {
      this.store.set(key, value);
    } catch (err) {
      console.error(`[ConfigStore] 写入配置失败: key=${key}`, err);
    }
  }

  /**
   * 获取所有配置
   */
  getAll(): Record<string, ConfigValue> {
    try {
      return this.store.store as Record<string, ConfigValue>;
    } catch (err) {
      console.error('[ConfigStore] 获取所有配置失败:', err);
      return {};
    }
  }

  /**
   * 重置指定配置到默认值
   * @param key 配置键
   */
  reset(key: string): void {
    try {
      const defaults = this.getDefaultConfig();
      if (key in defaults) {
        this.store.set(key, defaults[key]);
      } else {
        this.store.delete(key as keyof StoreSchema);
      }
    } catch (err) {
      console.error(`[ConfigStore] 重置配置失败: key=${key}`, err);
    }
  }

  /**
   * 获取 electron-store 原始实例
   */
  getRawStore(): ElectronStore<StoreSchema> {
    return this.store;
  }
}

/** 全局单例 */
export const configStore = new ElectronConfigStore();
