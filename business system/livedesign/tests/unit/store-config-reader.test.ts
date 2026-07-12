/**
 * StoreConfigReader 桥接适配器单元测试
 *
 * 覆盖：
 * 1. 注册路径 → ConfigStore 键名映射
 * 2. ConfigStore 返回值的透传
 * 3. undefined 值的透传（适配器三阶段降级兜底）
 * 4. 非前缀路径的透传
 * 5. ConfigStore 异常时返回 undefined
 * 6. 全链路集成：StoreConfigReader → CachedReader → Adapter
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import type { ConfigStore, ConfigValue } from '../../src/shared/config-schema';
import { StoreConfigReader } from '../../src/adapters/store-config-reader';
import { CachedReader } from '../../src/provider/cached-reader';
import { provideBlessStarAdapters } from '../../src/provider';
import { LiveDesignConfigAdapter } from '../../src/adapters/blessstar/livedesign-adapter';

// ==================== Mock ConfigStore ====================

class MockConfigStore implements ConfigStore {
  private values: Map<string, ConfigValue> = new Map();

  setValue(key: string, value: ConfigValue): void {
    this.values.set(key, value);
  }

  get<T extends ConfigValue>(key: string): T | undefined {
    return this.values.get(key) as T | undefined;
  }

  set<T extends ConfigValue>(key: string, value: T): void {
    this.values.set(key, value);
  }

  getAll(): Record<string, ConfigValue> {
    const result: Record<string, ConfigValue> = {};
    this.values.forEach((v, k) => { result[k] = v; });
    return result;
  }

  reset(key: string): void {
    this.values.delete(key);
  }
}

class ThrowingConfigStore implements ConfigStore {
  get<T extends ConfigValue>(_key: string): T | undefined {
    throw new Error('ConfigStore error');
  }
  set<T extends ConfigValue>(_key: string, _value: T): void {
    throw new Error('ConfigStore error');
  }
  getAll(): Record<string, ConfigValue> {
    throw new Error('ConfigStore error');
  }
  reset(_key: string): void {
    throw new Error('ConfigStore error');
  }
}

// ==================== 测试: 路径映射 ====================

describe('StoreConfigReader 路径映射', () => {
  it('应将注册路径转换为点号分隔的 ConfigStore 键名', async () => {
    const store = new MockConfigStore();
    store.setValue('avatar.position_x', 100);
    const reader = new StoreConfigReader(store);

    const val = await reader.get('/config/livedesign/avatar/position_x');
    expect(val).toBe(100);
  });

  it('应正确处理深层嵌套路径', async () => {
    const store = new MockConfigStore();
    store.setValue('ui.transparency', 0.8);
    const reader = new StoreConfigReader(store);

    const val = await reader.get('/config/livedesign/ui/transparency');
    expect(val).toBe(0.8);
  });

  it('应正确处理数组类型的配置值', async () => {
    const store = new MockConfigStore();
    store.setValue('assistant.tools', ['code_edit', 'file_browse', 'web_search']);
    const reader = new StoreConfigReader(store);

    const val = await reader.get('/config/livedesign/assistant/tools');
    expect(Array.isArray(val)).toBe(true);
    expect(val).toEqual(['code_edit', 'file_browse', 'web_search']);
  });

  it('非预期前缀的路径应直接传递', async () => {
    const store = new MockConfigStore();
    store.setValue('custom.key', 'custom-value');
    const reader = new StoreConfigReader(store);

    const val = await reader.get('custom.key');
    expect(val).toBe('custom-value');
  });

  it('ConfigStore 中不存在的路径应返回 undefined', async () => {
    const store = new MockConfigStore();
    const reader = new StoreConfigReader(store);

    const val = await reader.get('/config/livedesign/nonexistent/key');
    expect(val).toBeUndefined();
  });
});

// ==================== 测试: 返回值透传 ====================

describe('StoreConfigReader 返回值透传', () => {
  it('应返回 number 类型的配置值', async () => {
    const store = new MockConfigStore();
    store.setValue('avatar.position_x', 200);
    const reader = new StoreConfigReader(store);

    expect(await reader.get('/config/livedesign/avatar/position_x')).toBe(200);
  });

  it('应返回 string 类型的配置值', async () => {
    const store = new MockConfigStore();
    store.setValue('ui.theme', 'dark');
    const reader = new StoreConfigReader(store);

    expect(await reader.get('/config/livedesign/ui/theme')).toBe('dark');
  });

  it('应返回 boolean 类型的配置值', async () => {
    const store = new MockConfigStore();
    store.setValue('some.flag', false);
    const reader = new StoreConfigReader(store);

    expect(await reader.get('/config/livedesign/some/flag')).toBe(false);
  });

  it('应返回 string[] 类型的配置值', async () => {
    const store = new MockConfigStore();
    store.setValue('plugin.enabled', ['plugin-a', 'plugin-b']);
    const reader = new StoreConfigReader(store);

    const val = await reader.get('/config/livedesign/plugin/enabled');
    expect(val).toEqual(['plugin-a', 'plugin-b']);
  });
});

// ==================== 测试: 异常处理 ====================

describe('StoreConfigReader 异常处理', () => {
  it('ConfigStore 抛出异常时应返回 undefined', async () => {
    const store = new ThrowingConfigStore();
    const reader = new StoreConfigReader(store);

    const val = await reader.get('/config/livedesign/avatar/position_x');
    expect(val).toBeUndefined();
  });

  it('异常返回 undefined 后适配器应走三阶段降级', async () => {
    const store = new ThrowingConfigStore();
    const reader = new StoreConfigReader(store);
    const adapter = new LiveDesignConfigAdapter(reader);

    // ConfigStore 异常 → 适配器走阶段3：硬编码默认值
    expect(await adapter.avatarPositionX()).toBe(0);
    expect(await adapter.avatarScale()).toBe(1.0);
    expect(await adapter.uiTheme()).toBe('light');
    expect(await adapter.chatTemperature()).toBe(0.7);
  });
});

// ==================== 测试: 全链路集成 ====================

describe('StoreConfigReader 全链路集成', () => {
  let store: MockConfigStore;

  beforeEach(() => {
    store = new MockConfigStore();
    // 模拟 ConfigStore 中已有的配置值
    store.setValue('avatar.position_x', 500);
    store.setValue('avatar.position_y', 300);
    store.setValue('avatar.scale', 1.5);
    store.setValue('chat.role_preset', 'custom-role');
    store.setValue('chat.temperature', 0.3);
    store.setValue('assistant.tools', ['code_edit']);
    store.setValue('ui.theme', 'dark');
    store.setValue('ui.transparency', 0.5);
    store.setValue('plugin.enabled', ['my-plugin']);
  });

  it('StoreConfigReader → Adapter 应读取 ConfigStore 中的配置值', async () => {
    const reader = new StoreConfigReader(store);
    const adapter = new LiveDesignConfigAdapter(reader);

    expect(await adapter.avatarPositionX()).toBe(500);
    expect(await adapter.avatarPositionY()).toBe(300);
    expect(await adapter.avatarScale()).toBe(1.5);
    expect(await adapter.chatRolePreset()).toBe('custom-role');
    expect(await adapter.chatTemperature()).toBe(0.3);
    expect(await adapter.assistantTools()).toEqual(['code_edit']);
    expect(await adapter.uiTheme()).toBe('dark');
    expect(await adapter.uiTransparency()).toBe(0.5);
    expect(await adapter.pluginEnabled()).toEqual(['my-plugin']);
  });

  it('StoreConfigReader → CachedReader → Adapter 应正确工作', async () => {
    const rawReader = new StoreConfigReader(store);
    const cachedReader = new CachedReader(rawReader, 100);
    const adapter = new LiveDesignConfigAdapter(cachedReader);

    // 验证缓存层不影响正确读取
    expect(await adapter.avatarPositionX()).toBe(500);
    expect(await adapter.uiTheme()).toBe('dark');

    // 修改 ConfigStore 后，CachedReader 应返回缓存值（非实时穿透）
    store.setValue('avatar.position_x', 999);
    store.setValue('ui.theme', 'light');

    // CachedReader 缓存命中 → 返回旧值
    expect(await adapter.avatarPositionX()).toBe(500);
    expect(await adapter.uiTheme()).toBe('dark');

    cachedReader.destroy();
  });

  it('provideBlessStarAdapters → Adapters 应携带 liveDesignConfig 属性', () => {
    const reader = new StoreConfigReader(store);
    const adaptersInstance = provideBlessStarAdapters(reader);

    expect(adaptersInstance.liveDesignConfig).toBeDefined();
    expect(typeof adaptersInstance.liveDesignConfig.avatarPositionX).toBe('function');
  });
});
