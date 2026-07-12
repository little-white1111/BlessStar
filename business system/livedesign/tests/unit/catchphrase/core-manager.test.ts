/**
 * CoreCatchphraseManager 单元测试
 *
 * 架构不变量 D5: Core 口头禅纯配置驱动不落盘。
 */

import { describe, it, expect } from 'vitest';
import { CoreCatchphraseManager } from '../../../src/llm-service/catchphrase/core-manager';

describe('CoreCatchphraseManager（架构不变量 D5）', () => {
  it('无 ConfigReader 时应使用默认配置', async () => {
    const mgr = new CoreCatchphraseManager();
    await mgr.load();
    const all = mgr.getAll();
    expect(all).toHaveLength(2);
    expect(all[0].emotion_type).toBe('encouragement');
    expect(all[1].emotion_type).toBe('calming');
  });

  it('getCatchphrase 应返回正确的变体', async () => {
    const mgr = new CoreCatchphraseManager();
    await mgr.load();
    expect(mgr.getCatchphrase('encouragement', 'low')).toBe('加油');
    expect(mgr.getCatchphrase('encouragement', 'medium')).toBe('你一定可以的');
    expect(mgr.getCatchphrase('encouragement', 'high')).toBe('你一定可以做到的！');
    expect(mgr.getCatchphrase('calming', 'low')).toBe('没事的');
    expect(mgr.getCatchphrase('calming', 'medium')).toBe('没关系的~');
    expect(mgr.getCatchphrase('calming', 'high')).toBe('别担心，我一直在这里');
  });

  it('不存在的情绪类型应返回 undefined', async () => {
    const mgr = new CoreCatchphraseManager();
    await mgr.load();
    expect(mgr.getCatchphrase('nonexistent', 'low')).toBeUndefined();
  });

  it('resolveIntensityByDeviation 应返回正确档位', async () => {
    const mgr = new CoreCatchphraseManager();
    await mgr.load();
    expect(mgr.resolveIntensityByDeviation(0.2)).toBe('low');
    expect(mgr.resolveIntensityByDeviation(0.4)).toBe('medium');
    expect(mgr.resolveIntensityByDeviation(0.7)).toBe('high');
    expect(mgr.resolveIntensityByDeviation(0.3)).toBe('low'); // = 0.3, not >
    expect(mgr.resolveIntensityByDeviation(0.31)).toBe('medium');
  });

  it('isLoaded 应正确反映加载状态', async () => {
    const mgr = new CoreCatchphraseManager();
    expect(mgr.isLoaded()).toBe(false);
    await mgr.load();
    expect(mgr.isLoaded()).toBe(true);
  });

  it('reset 应清空所有数据', async () => {
    const mgr = new CoreCatchphraseManager();
    await mgr.load();
    expect(mgr.isLoaded()).toBe(true);
    mgr.reset();
    expect(mgr.isLoaded()).toBe(false);
    expect(mgr.getAll()).toHaveLength(0);
  });
});
