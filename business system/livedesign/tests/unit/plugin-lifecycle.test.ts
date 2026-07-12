/**
 * 插件生命周期单元测试
 * 测试 LifecycleManager 的状态转换和生命周期方法
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { LifecycleManager, PluginInstance } from '../../src/plugin-host/lifecycle-manager';
import { PluginManifest, PluginState, Plugin } from '../../src/shared/plugin-interface';

/**
 * 创建一个模拟插件实例
 */
function createMockPlugin(): Plugin {
  return {
    init: async (_config: Record<string, unknown>) => {},
    start: async () => {},
    stop: async () => {},
    destroy: async () => {},
  };
}

/**
 * 创建一个最小的 PluginManifest
 */
function createManifest(id: string = 'test-plugin'): PluginManifest {
  return {
    id,
    name: '测试插件',
    version: '1.0.0',
    description: '用于单元测试的插件',
    author: 'test',
    permissions: [],
    main: 'index.js',
  };
}

describe('LifecycleManager', () => {
  let manager: LifecycleManager;

  beforeEach(() => {
    manager = new LifecycleManager();
  });

  describe('register', () => {
    it('应成功注册一个新插件', () => {
      const manifest = createManifest();
      manager.register(manifest);

      const plugin = manager.get(manifest.id);
      expect(plugin).toBeDefined();
      expect(plugin!.state).toBe(PluginState.INSTALLED);
      expect(plugin!.instance).toBeNull();
      expect(plugin!.manifest).toEqual(manifest);
    });

    it('重复注册同一插件应报错', () => {
      const manifest = createManifest();
      manager.register(manifest);

      expect(() => manager.register(manifest)).toThrow('已注册');
    });
  });

  describe('get / getAll', () => {
    it('get 应返回已注册的插件实例', () => {
      const manifest = createManifest('plugin-a');
      manager.register(manifest);

      const plugin = manager.get('plugin-a');
      expect(plugin).toBeDefined();
      expect(plugin!.manifest.id).toBe('plugin-a');
    });

    it('get 不存在的插件应返回 undefined', () => {
      expect(manager.get('nonexistent')).toBeUndefined();
    });

    it('getAll 应返回所有已注册的插件', () => {
      manager.register(createManifest('a'));
      manager.register(createManifest('b'));

      const all = manager.getAll();
      expect(all).toHaveLength(2);
    });
  });

  describe('生命周期状态转换', () => {
    it('正常流程: INSTALLED → LOADING → RUNNING → STOPPED', async () => {
      const manifest = createManifest();
      manager.register(manifest);

      // 创建模拟实例并注入
      const plugin = manager.get(manifest.id)!;
      plugin.instance = createMockPlugin();

      // INSTALLED → LOADING (init)
      await manager.init(manifest.id, {});
      expect(manager.get(manifest.id)!.state).toBe(PluginState.LOADING);

      // LOADING → RUNNING (start)
      await manager.start(manifest.id);
      expect(manager.get(manifest.id)!.state).toBe(PluginState.RUNNING);

      // RUNNING → STOPPED (stop)
      await manager.stop(manifest.id);
      expect(manager.get(manifest.id)!.state).toBe(PluginState.STOPPED);
    });

    it('INSTALLED → RUNNING 非法状态转换应报错', async () => {
      const manifest = createManifest();
      manager.register(manifest);

      const plugin = manager.get(manifest.id)!;
      plugin.instance = createMockPlugin();

      // 从 INSTALLED 直接 start 应报错（start 方法要求 LOADING 状态）
      await expect(manager.start(manifest.id)).rejects.toThrow('需要 LOADING 状态才能 start');
      expect(manager.get(manifest.id)!.state).toBe(PluginState.INSTALLED);
    });

    it('STOPPED → RUNNING 非法状态转换应报错', async () => {
      const manifest = createManifest();
      manager.register(manifest);

      const plugin = manager.get(manifest.id)!;
      plugin.instance = createMockPlugin();

      await manager.init(manifest.id, {});
      await manager.start(manifest.id);
      await manager.stop(manifest.id);

      // 从 STOPPED 直接 start 应报错（start 方法要求 LOADING 状态）
      await expect(manager.start(manifest.id)).rejects.toThrow('需要 LOADING 状态才能 start');
    });
  });

  describe('init 方法', () => {
    it('未注册的插件 init 应报错', async () => {
      await expect(manager.init('unknown', {})).rejects.toThrow('未注册');
    });

    it('状态不是 INSTALLED 时 init 应报错', async () => {
      const manifest = createManifest();
      manager.register(manifest);
      const plugin = manager.get(manifest.id)!;
      plugin.instance = createMockPlugin();

      // 先 init 一次
      await manager.init(manifest.id, {});

      // 再次 init 应报错（当前为 LOADING）
      await expect(manager.init(manifest.id, {})).rejects.toThrow('需要 INSTALLED 状态');
    });

    it('init 时 instance 为 null 应报错', async () => {
      const manifest = createManifest();
      manager.register(manifest);
      // 不设置 instance

      await expect(manager.init(manifest.id, {})).rejects.toThrow('实例未加载');
    });

    it('init 失败时应回滚到 ERROR 状态', async () => {
      const manifest = createManifest();
      manager.register(manifest);

      const failingPlugin: Plugin = {
        init: async () => { throw new Error('初始化失败'); },
        start: async () => {},
        stop: async () => {},
        destroy: async () => {},
      };

      const plugin = manager.get(manifest.id)!;
      plugin.instance = failingPlugin;

      await expect(manager.init(manifest.id, {})).rejects.toThrow('初始化失败');
      expect(plugin.state).toBe(PluginState.ERROR);
      expect(plugin.error).toContain('初始化失败');
    });
  });

  describe('start 方法', () => {
    it('未注册的插件 start 应报错', async () => {
      await expect(manager.start('unknown')).rejects.toThrow('未注册');
    });

    it('start 失败时应回滚到 STOPPED 状态', async () => {
      const manifest = createManifest();
      manager.register(manifest);

      const failingPlugin: Plugin = {
        init: async () => {},
        start: async () => { throw new Error('启动失败'); },
        stop: async () => {},
        destroy: async () => {},
      };

      const plugin = manager.get(manifest.id)!;
      plugin.instance = failingPlugin;

      await manager.init(manifest.id, {});
      await expect(manager.start(manifest.id)).rejects.toThrow('启动失败');

      expect(plugin.state).toBe(PluginState.STOPPED);
      expect(plugin.error).toContain('启动失败');
    });
  });

  describe('stop 方法', () => {
    it('未注册的插件 stop 应报错', async () => {
      await expect(manager.stop('unknown')).rejects.toThrow('未注册');
    });

    it('从 RUNNING 状态 stop 应成功', async () => {
      const manifest = createManifest();
      manager.register(manifest);
      const plugin = manager.get(manifest.id)!;
      plugin.instance = createMockPlugin();

      await manager.init(manifest.id, {});
      await manager.start(manifest.id);
      await manager.stop(manifest.id);

      expect(plugin.state).toBe(PluginState.STOPPED);
    });

    it('stop 内部发生错误仍应切换为 STOPPED 并记录 error', async () => {
      const manifest = createManifest();
      manager.register(manifest);

      const failingPlugin: Plugin = {
        init: async () => {},
        start: async () => {},
        stop: async () => { throw new Error('停止出错'); },
        destroy: async () => {},
      };

      const plugin = manager.get(manifest.id)!;
      plugin.instance = failingPlugin;

      await manager.init(manifest.id, {});
      await manager.start(manifest.id);
      await manager.stop(manifest.id);

      expect(plugin.state).toBe(PluginState.STOPPED);
      expect(plugin.error).toContain('停止时发生错误');
    });
  });

  describe('destroy 方法', () => {
    it('destroy 后 instance 应置为 null', async () => {
      const manifest = createManifest();
      manager.register(manifest);
      const plugin = manager.get(manifest.id)!;
      plugin.instance = createMockPlugin();

      await manager.init(manifest.id, {});
      await manager.start(manifest.id);

      await manager.destroy(manifest.id);

      expect(plugin.instance).toBeNull();
      expect(plugin.state).toBe(PluginState.INSTALLED);
    });

    it('未注册的插件 destroy 应报错', async () => {
      await expect(manager.destroy('unknown')).rejects.toThrow('未注册');
    });

    it('destroy 不存在的 instance 不应报错', async () => {
      const manifest = createManifest();
      manager.register(manifest);
      // instance 为 null

      // destroy 应能正常执行
      await manager.destroy(manifest.id);
      const plugin = manager.get(manifest.id)!;
      expect(plugin.instance).toBeNull();
    });
  });

  describe('destroyAll 方法', () => {
    it('应销毁所有已注册的插件', async () => {
      manager.register(createManifest('plugin-a'));
      manager.register(createManifest('plugin-b'));

      const pluginA = manager.get('plugin-a')!;
      const pluginB = manager.get('plugin-b')!;
      pluginA.instance = createMockPlugin();
      pluginB.instance = createMockPlugin();

      await manager.destroyAll();

      expect(pluginA.instance).toBeNull();
      expect(pluginB.instance).toBeNull();
    });
  });

  describe('unregister 方法', () => {
    it('未注册的插件 unregister 应报错', () => {
      expect(() => manager.unregister('unknown')).toThrow('未注册');
    });

    it('实例仍活跃时 unregister 应报错', () => {
      const manifest = createManifest();
      manager.register(manifest);
      manager.get(manifest.id)!.instance = createMockPlugin();

      expect(() => manager.unregister(manifest.id)).toThrow('仍有活跃实例');
    });

    it('destroy 后应能成功 unregister', async () => {
      const manifest = createManifest();
      manager.register(manifest);
      manager.get(manifest.id)!.instance = createMockPlugin();

      await manager.destroy(manifest.id);
      manager.unregister(manifest.id);

      expect(manager.get(manifest.id)).toBeUndefined();
    });
  });
});
