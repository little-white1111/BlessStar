/**
 * ConfigWatcher — 文件变更监听器
 * 检测外部编辑对 component.yaml 的修改，触发配置重载和热更新
 * 遵循架构不变量 I6：文件变更（外部编辑）必须触发组件热更新
 *
 * 支持两种模式：
 * 1. fs.watch（Node.js 原生，高效）
 * 2. 轮询回退（fs.watch 不可用时）
 */

import { watch, existsSync, statSync, readdirSync } from 'fs';
import { resolve, dirname, basename } from 'path';

export interface ConfigChangeEvent {
  /** 变更的文件绝对路径 */
  filePath: string;
  /** 目录名（组件标识） */
  dirName: string;
  /** 变更类型：'change' | 'rename' | 'delete' */
  type: 'change' | 'rename' | 'delete';
  /** 变更时间戳 */
  timestamp: number;
}

export type ConfigChangeCallback = (event: ConfigChangeEvent) => void;

export interface ConfigWatcherOptions {
  /** 轮询间隔（ms），fs.watch 不可用时使用，默认 3000 */
  pollIntervalMs?: number;
  /** 变更事件防抖间隔（ms），默认 500 */
  debounceMs?: number;
}

/**
 * 组件配置文件监听器
 * 监听指定目录下的 component.yaml 变更，触发热更新回调
 */
export class ConfigWatcher {
  private watchDir: string;
  private callback: ConfigChangeCallback | null = null;
  private watcher: ReturnType<typeof watch> | null = null;
  private pollTimer: ReturnType<typeof setInterval> | null = null;
  private options: Required<ConfigWatcherOptions>;
  private lastEventTimestamps = new Map<string, number>();
  private debounceTimers = new Map<string, ReturnType<typeof setTimeout>>();

  constructor(watchDir: string, options?: ConfigWatcherOptions) {
    this.watchDir = resolve(watchDir);
    this.options = {
      pollIntervalMs: options?.pollIntervalMs ?? 3000,
      debounceMs: options?.debounceMs ?? 500,
    };
  }

  /**
   * 注册文件变更回调
   */
  onConfigChange(callback: ConfigChangeCallback): void {
    this.callback = callback;
  }

  /**
   * 启动监听
   */
  start(): void {
    if (this.watcher || this.pollTimer) return; // 防止重复启动

    // 确保被监听的目录存在
    if (!existsSync(this.watchDir)) {
      console.warn(`[ConfigWatcher] 目录不存在，等待创建: ${this.watchDir}`);
    }

    // 优先使用 fs.watch（Node.js 原生）
    try {
      this.watcher = watch(
        this.watchDir,
        { recursive: true },
        (eventType, filename) => {
          if (!filename || !this.isComponentConfig(filename)) return;

          // 防抖处理
          const now = Date.now();
          const last = this.lastEventTimestamps.get(filename) ?? 0;
          if (now - last < this.options.debounceMs) {
            // 重置防抖定时器
            const existingTimer = this.debounceTimers.get(filename);
            if (existingTimer) clearTimeout(existingTimer);
          }

          this.lastEventTimestamps.set(filename, now);

          const timer = setTimeout(() => {
            this.debounceTimers.delete(filename);
            this.emitChange(filename, eventType as ConfigChangeEvent['type']);
          }, this.options.debounceMs);
          this.debounceTimers.set(filename, timer);
        },
      );
      console.log(`[ConfigWatcher] fs.watch 已启动: ${this.watchDir}`);
    } catch (err) {
      console.warn(`[ConfigWatcher] fs.watch 不可用，回退到轮询模式: ${(err as Error).message}`);
      this.startPolling();
    }
  }

  /**
   * 停止监听
   */
  stop(): void {
    if (this.watcher) {
      this.watcher.close();
      this.watcher = null;
    }
    if (this.pollTimer) {
      clearInterval(this.pollTimer);
      this.pollTimer = null;
    }
    // 清理所有防抖定时器
    for (const timer of this.debounceTimers.values()) {
      clearTimeout(timer);
    }
    this.debounceTimers.clear();
    this.lastEventTimestamps.clear();
    console.log(`[ConfigWatcher] 已停止: ${this.watchDir}`);
  }

  /**
   * 标记强制使用轮询模式（测试用）
   */
  forcePolling(): void {
    if (this.watcher) {
      this.watcher.close();
      this.watcher = null;
    }
    this.startPolling();
  }

  // ========== 私有方法 ==========

  /**
   * 启动轮询模式（fs.watch 不可用时的回退方案）
   */
  private startPolling(): void {
    const snapshots = new Map<string, number>();

    // 初始快照
    this.takeSnapshot(snapshots);

    this.pollTimer = setInterval(() => {
      const currentSnap = new Map<string, number>();
      this.takeSnapshot(currentSnap);

      for (const [filePath, mtime] of currentSnap) {
        const prevMtime = snapshots.get(filePath);
        if (prevMtime !== undefined && mtime > prevMtime && this.isComponentConfig(filePath)) {
          // 文件有变更，防抖处理
          const now = Date.now();
          const last = this.lastEventTimestamps.get(filePath) ?? 0;
          if (now - last >= this.options.debounceMs) {
            this.lastEventTimestamps.set(filePath, now);
            this.emitChange(filePath, 'change');
          }
        }
        snapshots.set(filePath, mtime);
      }
    }, this.options.pollIntervalMs);
  }

  /**
   * 拍摄目录文件快照（mtime）
   */
  private takeSnapshot(snapshots: Map<string, number>): void {
    if (!existsSync(this.watchDir)) return;

    try {
      const entries = readdirSync(this.watchDir, { withFileTypes: true });
      for (const entry of entries) {
        if (!entry.isDirectory()) continue;
        const componentYamlPath = resolve(this.watchDir, entry.name, 'component.yaml');
        if (existsSync(componentYamlPath)) {
          snapshots.set(componentYamlPath, statSync(componentYamlPath).mtimeMs);
        }
      }
    } catch {
      // 目录不可读时静默跳过
    }
  }

  /**
   * 判断文件名是否为组件配置文件
   */
  private isComponentConfig(filename: string): boolean {
    // 只关注 component.yaml 文件
    return basename(filename) === 'component.yaml';
  }

  /**
   * 触发变更回调
   */
  private emitChange(filename: string, type: ConfigChangeEvent['type']): void {
    if (!this.callback) return;

    const filePath = resolve(this.watchDir, filename);
    const dirName = dirname(filename);

    this.callback({
      filePath,
      dirName,
      type,
      timestamp: Date.now(),
    });
  }
}
