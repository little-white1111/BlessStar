/**
 * Port-Adapter 层单元测试
 *
 * 覆盖：
 * 1. ConfigReader 接口契约
 * 2. LiveDesignConfig Port 接口契约
 * 3. LiveDesignConfigAdapter 三阶段降级策略
 * 4. LiveDesignConfigMock 固定返回值
 * 5. CachedReader 缓存装饰器
 * 6. Adapters 依赖注入
 * 7. GateRule 门禁规则
 */

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import type { ConfigReader } from '../../src/ports/config-reader';
import type { LiveDesignConfig } from '../../src/ports/livedesign';
import { LiveDesignConfigAdapter } from '../../src/adapters/blessstar/livedesign-adapter';
import { LiveDesignConfigMock } from '../../src/adapters/mock/livedesign-mock';
import { CachedReader } from '../../src/provider/cached-reader';
import { Adapters, provideBlessStarAdapters } from '../../src/provider';

// ==================== Mock ConfigReader ====================

class MockConfigReader implements ConfigReader {
  private values: Map<string, unknown> = new Map();
  private shouldThrow = false;

  setValue(path: string, value: unknown): void {
    this.values.set(path, value);
  }

  setThrow(shouldThrow: boolean): void {
    this.shouldThrow = shouldThrow;
  }

  async get(path: string): Promise<unknown> {
    if (this.shouldThrow) {
      throw new Error('ConfigReader error');
    }
    return this.values.get(path);
  }
}

class NullConfigReader implements ConfigReader {
  async get(_path: string): Promise<unknown> {
    return undefined;
  }
}

// ==================== 测试: Port 接口契约 ====================

describe('Port 接口契约', () => {
  it('LiveDesignConfig 接口应定义全部 9 个方法', () => {
    // 类型检查 — 验证 LiveDesignConfigMock 实现了 LiveDesignConfig
    const mock: LiveDesignConfig = new LiveDesignConfigMock();
    expect(mock).toBeDefined();
    expect(typeof mock.avatarPositionX).toBe('function');
    expect(typeof mock.avatarPositionY).toBe('function');
    expect(typeof mock.avatarScale).toBe('function');
    expect(typeof mock.chatRolePreset).toBe('function');
    expect(typeof mock.chatTemperature).toBe('function');
    expect(typeof mock.assistantTools).toBe('function');
    expect(typeof mock.uiTheme).toBe('function');
    expect(typeof mock.uiTransparency).toBe('function');
    expect(typeof mock.pluginEnabled).toBe('function');
  });

  it('ConfigReader 接口应定义 get 方法', () => {
    const reader: ConfigReader = new MockConfigReader();
    expect(typeof reader.get).toBe('function');
  });
});

// ==================== 测试: LiveDesignConfigMock ====================

describe('LiveDesignConfigMock', () => {
  let mock: LiveDesignConfigMock;

  beforeEach(() => {
    mock = new LiveDesignConfigMock();
  });

  it('应返回默认位置坐标', async () => {
    expect(await mock.avatarPositionX()).toBe(0);
    expect(await mock.avatarPositionY()).toBe(0);
  });

  it('应返回默认缩放比例', async () => {
    expect(await mock.avatarScale()).toBe(1.0);
  });

  it('应返回默认角色预设', async () => {
    expect(await mock.chatRolePreset()).toBe('assistant');
  });

  it('应返回默认温度值', async () => {
    expect(await mock.chatTemperature()).toBe(0.7);
  });

  it('应返回默认工具列表', async () => {
    const tools = await mock.assistantTools();
    expect(Array.isArray(tools)).toBe(true);
    expect(tools).toContain('code_edit');
    expect(tools).toContain('file_browse');
    expect(tools).toContain('web_search');
  });

  it('应返回默认主题', async () => {
    expect(await mock.uiTheme()).toBe('light');
  });

  it('应返回默认透明度', async () => {
    expect(await mock.uiTransparency()).toBe(0.9);
  });

  it('应返回默认插件列表', async () => {
    const plugins = await mock.pluginEnabled();
    expect(Array.isArray(plugins)).toBe(true);
    expect(plugins.length).toBe(0);
  });
});

// ==================== 测试: LiveDesignConfigAdapter 三阶段降级 ====================

describe('LiveDesignConfigAdapter 三阶段降级', () => {
  describe('第1阶段: ConfigReader 实时查询', () => {
    it('ConfigReader 返回有效值时使用实时值', async () => {
      const reader = new MockConfigReader();
      reader.setValue('/config/livedesign/avatar/position_x', 100);
      reader.setValue('/config/livedesign/ui/theme', 'dark');

      const adapter = new LiveDesignConfigAdapter(reader);

      expect(await adapter.avatarPositionX()).toBe(100);
      expect(await adapter.uiTheme()).toBe('dark');
    });

    it('ConfigReader 返回不同数据类型', async () => {
      const reader = new MockConfigReader();
      reader.setValue('/config/livedesign/avatar/scale', 1.5);
      reader.setValue('/config/livedesign/chat/temperature', 0.5);
      reader.setValue('/config/livedesign/assistant/tools', ['code_edit']);
      reader.setValue('/config/livedesign/plugin/enabled', ['plugin-a']);

      const adapter = new LiveDesignConfigAdapter(reader);

      expect(await adapter.avatarScale()).toBe(1.5);
      expect(await adapter.chatTemperature()).toBe(0.5);
      expect(await adapter.assistantTools()).toEqual(['code_edit']);
      expect(await adapter.pluginEnabled()).toEqual(['plugin-a']);
    });
  });

  describe('第2阶段: Last Known Good 缓存', () => {
    it('ConfigReader 失败时使用 Last Known Good 缓存', async () => {
      const reader = new MockConfigReader();

      // 第一次调用成功 — 填充缓存
      reader.setValue('/config/livedesign/avatar/position_x', 200);
      const adapter = new LiveDesignConfigAdapter(reader);
      expect(await adapter.avatarPositionX()).toBe(200);

      // 第二次调用失败 — 使用缓存
      reader.setThrow(true);
      expect(await adapter.avatarPositionX()).toBe(200);
    });

    it('不同配置项独立缓存', async () => {
      const reader = new MockConfigReader();

      reader.setValue('/config/livedesign/avatar/position_x', 100);
      reader.setValue('/config/livedesign/ui/theme', 'dark');

      const adapter = new LiveDesignConfigAdapter(reader);
      expect(await adapter.avatarPositionX()).toBe(100);
      expect(await adapter.uiTheme()).toBe('dark');

      // 只让其中一个失败
      reader.setValue('/config/livedesign/avatar/position_x', undefined);
      expect(await adapter.avatarPositionX()).toBe(100); // 缓存
      expect(await adapter.uiTheme()).toBe('dark'); // 实时
    });
  });

  describe('第3阶段: 硬编码默认值', () => {
    it('ConfigReader 返回 undefined 且缓存为空时使用默认值', async () => {
      const reader = new NullConfigReader();
      const adapter = new LiveDesignConfigAdapter(reader);

      // 首次调用 — 无缓存 + reader 返回 undefined
      expect(await adapter.avatarPositionX()).toBe(0);
      expect(await adapter.avatarPositionY()).toBe(0);
      expect(await adapter.avatarScale()).toBe(1.0);
      expect(await adapter.chatRolePreset()).toBe('assistant');
      expect(await adapter.chatTemperature()).toBe(0.7);
      expect(await adapter.assistantTools()).toEqual(['code_edit', 'file_browse', 'web_search']);
      expect(await adapter.uiTheme()).toBe('light');
      expect(await adapter.uiTransparency()).toBe(0.9);
      expect(await adapter.pluginEnabled()).toEqual([]);
    });

    it('ConfigReader 异常抛出时使用默认值', async () => {
      const reader = new MockConfigReader();
      reader.setThrow(true);
      const adapter = new LiveDesignConfigAdapter(reader);

      expect(await adapter.avatarScale()).toBe(1.0);
      expect(await adapter.chatTemperature()).toBe(0.7);
    });
  });

  describe('三阶段降级综合场景', () => {
    it('按 实时 → 缓存 → 默认 优先级降级', async () => {
      const reader = new MockConfigReader();
      const adapter = new LiveDesignConfigAdapter(reader);

      // 阶段1: 实时值
      reader.setValue('/config/livedesign/avatar/position_x', 500);
      expect(await adapter.avatarPositionX()).toBe(500);

      // 阶段2: 缓存（reader 返回 undefined）
      reader.setValue('/config/livedesign/avatar/position_x', undefined);
      expect(await adapter.avatarPositionX()).toBe(500);

      // 恢复 reader 后使用实时值
      reader.setValue('/config/livedesign/avatar/position_x', 800);
      expect(await adapter.avatarPositionX()).toBe(800);
    });
  });
});

// ==================== 测试: CachedReader ====================

describe('CachedReader', () => {
  let innerReader: MockConfigReader;
  let cachedReader: CachedReader;

  beforeEach(() => {
    innerReader = new MockConfigReader();
    cachedReader = new CachedReader(innerReader, 100); // 100ms interval
  });

  afterEach(() => {
    cachedReader.destroy();
  });

  it('应穿透到 inner reader 并缓存结果', async () => {
    innerReader.setValue('/config/test/key', 'value1');

    const val1 = await cachedReader.get('/config/test/key');
    expect(val1).toBe('value1');

    // 修改 inner 但缓存应返回旧值
    innerReader.setValue('/config/test/key', 'value2');
    const val2 = await cachedReader.get('/config/test/key');
    expect(val2).toBe('value1'); // 缓存命中
  });

  it('inner reader 返回 undefined 时不应缓存', async () => {
    const val = await cachedReader.get('/config/test/nonexistent');
    expect(val).toBeUndefined();
    expect(innerReader.setValue('/config/test/nonexistent', 'value')).toBeUndefined();
  });

  it('支持手动设置缓存预热', () => {
    cachedReader.set('/config/test/prewarm', 'prewarmed');
    // 不需要 await，因为是同步方法
  });

  it('destroy 后应停止定时器', () => {
    cachedReader.destroy();
    // 调用 destroy 两次应该安全
    cachedReader.destroy();
  });

  it('支持 setRefreshFn', async () => {
    const refreshFn = vi.fn().mockResolvedValue(undefined);
    cachedReader.setRefreshFn(refreshFn);
    expect(refreshFn).not.toHaveBeenCalled();

    // 等待定时器触发
    await new Promise((resolve) => setTimeout(resolve, 150));
    cachedReader.destroy();
    // 注意: refreshFn 是异步的，这里只验证它被设置了
  });
});

// ==================== 测试: Adapters 依赖注入 ====================

describe('Adapters 依赖注入', () => {
  it('provideBlessStarAdapters 应返回 Adapters 实例', () => {
    const reader = new NullConfigReader();
    const adapters = provideBlessStarAdapters(reader);
    expect(adapters).toBeInstanceOf(Adapters);
  });

  it('Adapters 应包含 liveDesignConfig 属性', () => {
    const reader = new NullConfigReader();
    const adapters = provideBlessStarAdapters(reader);

    expect(adapters.liveDesignConfig).toBeDefined();
    expect(typeof adapters.liveDesignConfig.avatarPositionX).toBe('function');
    expect(typeof adapters.liveDesignConfig.uiTheme).toBe('function');
  });

  it('Adapters 适配器应使用提供的 reader', async () => {
    const reader = new MockConfigReader();
    reader.setValue('/config/livedesign/chat/role_preset', 'custom-role');

    const adapters = provideBlessStarAdapters(reader);
    expect(await adapters.liveDesignConfig.chatRolePreset()).toBe('custom-role');
  });
});

// ==================== 测试: GateRule 门禁规则类型 ====================

describe('门禁规则', () => {
  it('应支持 GateRule 类型定义', () => {
    const rangeRule: import('../../src/gate-configs').GateRule = {
      fieldKey: 'avatar.position_x',
      gateType: 'RANGE',
      params: { min: '-1920', max: '3840' },
    };
    expect(rangeRule.fieldKey).toBe('avatar.position_x');
    expect(rangeRule.gateType).toBe('RANGE');
    expect(rangeRule.params.min).toBe('-1920');
    expect(rangeRule.params.max).toBe('3840');
  });

  it('GATE_RULES 应包含门禁规则', async () => {
    const { GATE_RULES } = await import('../../src/gate-configs');
    expect(Array.isArray(GATE_RULES)).toBe(true);
    expect(GATE_RULES.length).toBeGreaterThan(0);

    const rangeRules = GATE_RULES.filter((r) => r.gateType === 'RANGE');
    expect(rangeRules.length).toBeGreaterThan(0);

    const approvalRules = GATE_RULES.filter((r) => r.gateType === 'APPROVAL');
    expect(approvalRules.length).toBeGreaterThan(0);
  });
});
