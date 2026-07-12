/**
 * 此文件由 @blessstar/codegen-ts 自动生成
 * 源文件: ..\..\config-schema.yaml
 * 生成时间: 2026-07-11
 * 请勿手动编辑 — 下次运行 codegen 时将覆盖
 */

import type { LivestyleConfig } from './livestyle-config.types';
import type { ConfigPort } from './livestyle-config.port';

/**
 * MockConfigAdapter — 测试用 Mock 实现
 * 所有配置项使用 config-schema.yaml 中定义的默认值
 */
export class MockConfigAdapter implements ConfigPort {
  private config: LivestyleConfig;
  private listeners: Map<string, Array<(newVal: unknown, oldVal: unknown) => void>>;

  constructor() {
    this.listeners = new Map();
    this.config = this.createDefaultConfig();
  }

  private createDefaultConfig(): LivestyleConfig {
    return {
      'canvas.grid.size': 20,
      'canvas.grid.snap': true,
      'canvas.default_width': 1920,
      'canvas.default_height': 1080,
      'canvas.autosave_interval_ms': 30000,
      'component.default_width': 400,
      'component.default_height': 300,
      'engine.render.fps_limit': 60,
      'engine.render.dirty_rect_enabled': true,
      'obs.connection.host': 'localhost',
      'obs.connection.port': 4455,
      'obs.connection.password': '',
      'obs.connection.auto_reconnect': true,
      'obs.connection.retry_interval_ms': 5000,
      'obs.sync.debounce_ms': 200,
      'component.task-list.title': '任务列表',
      'component.task-list.backgroundColor': '#1a1a2e',
      'component.task-list.fontSize': 14,
      'component.task-list.textColor': '#ffffff',
      'component.task-list.items': '任务1,任务2,任务3',
      'component.task-list.showHeader': true,
      'component.task-list.borderRadius': 8,
    };
  }

  get<K extends keyof LivestyleConfig>(key: K): LivestyleConfig[K] {
    return this.config[key];
  }

  getAll(): LivestyleConfig {
    return { ...this.config };
  }

  set<K extends keyof LivestyleConfig>(key: K, value: LivestyleConfig[K]): void {
    const oldValue = this.config[key];
    this.config[key] = value;
    this.notify(key as string, value, oldValue);
  }

  subscribe<K extends keyof LivestyleConfig>(
    key: K | '*',
    listener: (newValue: LivestyleConfig[K], oldValue: LivestyleConfig[K]) => void,
  ): () => void {
    const key_ = key as string;
    if (!this.listeners.has(key_)) {
      this.listeners.set(key_, []);
    }
    this.listeners.get(key_)!.push(listener as (newVal: unknown, oldVal: unknown) => void);
    return () => {
      const arr = this.listeners.get(key_);
      if (arr) {
        const idx = arr.indexOf(listener as (newVal: unknown, oldVal: unknown) => void);
        if (idx >= 0) arr.splice(idx, 1);
      }
    };
  }

  private notify(key: string, newValue: unknown, oldValue: unknown): void {
    const exact = this.listeners.get(key);
    if (exact) {
      exact.forEach(fn => fn(newValue, oldValue));
    }
    const wildcard = this.listeners.get('*');
    if (wildcard) {
      wildcard.forEach(fn => fn(newValue, oldValue));
    }
  }
}
