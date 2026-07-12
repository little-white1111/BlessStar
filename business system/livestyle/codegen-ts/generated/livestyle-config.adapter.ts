/**
 * 此文件由 @blessstar/codegen-ts 自动生成
 * 源文件: ..\..\config-schema.yaml
 * 生成时间: 2026-07-11
 * 请勿手动编辑 — 下次运行 codegen 时将覆盖
 */

import * as fs from 'fs';
import * as path from 'path';
import type { LivestyleConfig } from './livestyle-config.types';
import type { ConfigPort } from './livestyle-config.port';

/**
 * LocalFileConfigAdapter — 基于本地 JSON 文件的配置适配器
 * 配置存储路径可通过构造函数参数指定，默认在 process.cwd() 下
 */
export class LocalFileConfigAdapter implements ConfigPort {
  private config: LivestyleConfig;
  private filePath: string;
  private listeners: Map<string, Array<(newVal: unknown, oldVal: unknown) => void>>;

  constructor(filePath?: string) {
    this.listeners = new Map();
    this.filePath = filePath ?? path.resolve(process.cwd(), 'livestyle-config.json');
    this.config = this.loadFromFile();
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

  private loadFromFile(): LivestyleConfig {
    try {
      if (fs.existsSync(this.filePath)) {
        const raw = fs.readFileSync(this.filePath, 'utf-8');
        const parsed = JSON.parse(raw);
        return { ...this.createDefaultConfig(), ...parsed };
      }
    } catch {
      // 文件读取失败时使用默认值
    }
    return this.createDefaultConfig();
  }

  private persist(): void {
    try {
      const dir = path.dirname(this.filePath);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }
      fs.writeFileSync(this.filePath, JSON.stringify(this.config, null, 2), 'utf-8');
    } catch (err) {
      console.error('[LocalFileConfigAdapter] 配置持久化失败:', err);
    }
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
    this.persist();
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
