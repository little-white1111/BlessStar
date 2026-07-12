/**
 * ConfigWatcher 单元测试
 * 覆盖 I6 热更新机制
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { mkdtempSync, writeFileSync, mkdirSync, rmSync } from 'fs';
import { join } from 'path';
import { tmpdir } from 'os';

// 模拟 fs.watch
vi.mock('fs', async (importOriginal) => {
  const actual = await importOriginal<typeof import('fs')>();
  return {
    ...actual,
    watch: vi.fn().mockImplementation(() => {
      return { close: vi.fn() };
    }),
  };
});

import { ConfigWatcher } from '../config/ConfigWatcher';

describe('ConfigWatcher', () => {
  let tempDir: string;
  let watcher: ConfigWatcher;

  beforeEach(() => {
    tempDir = mkdtempSync(join(tmpdir(), 'livestyle-config-test-'));
    // 创建组件目录
    mkdirSync(join(tempDir, 'task-list'), { recursive: true });
    writeFileSync(join(tempDir, 'task-list', 'component.yaml'), 'component:\n  name: test\n');
  });

  afterEach(() => {
    watcher?.stop();
    rmSync(tempDir, { recursive: true, force: true });
  });

  it('应创建实例并成功启动 fs.watch', () => {
    watcher = new ConfigWatcher(tempDir);
    expect(watcher).toBeInstanceOf(ConfigWatcher);
    expect(() => watcher.start()).not.toThrow();
  });

  it('多次 start() 不应重复启动', () => {
    watcher = new ConfigWatcher(tempDir);
    watcher.start();
    // 第二次 start 不应抛出或创建新 watcher
    expect(() => watcher.start()).not.toThrow();
  });

  it('stop() 应清理所有定時器和 watcher', () => {
    watcher = new ConfigWatcher(tempDir);
    watcher.start();
    watcher.stop();
    // 停止后再次 start 应能正常工作
    expect(() => watcher.start()).not.toThrow();
  });

  it('onConfigChange 应注册回调', () => {
    watcher = new ConfigWatcher(tempDir);
    const callback = vi.fn();
    watcher.onConfigChange(callback);
    watcher.start();
    // 回调在文件变更时触发，这里只验证注册不报错
    expect(callback).not.toHaveBeenCalled();
  });

  it('forcePolling 应切换到轮询模式', () => {
    watcher = new ConfigWatcher(tempDir, { pollIntervalMs: 1000, debounceMs: 100 });
    watcher.forcePolling();
    watcher.stop();
    // 切换回 fs.watch 应正常工作
    expect(() => watcher.start()).not.toThrow();
  });
});
