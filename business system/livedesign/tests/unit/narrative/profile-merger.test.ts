/**
 * L4 动态画像合成器单元测试
 *
 * 架构不变量验证：
 *   B3 — 低于 0.4 置信度的假设不写入画像
 *   B4 — L4 动态画像不可直接用于在线对话决策（标记说明）
 *   B6 — 画像携带 version 时间戳
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { MemoryStore } from '../../../src/llm-service/narrative/storage/sqlite-store';
import { ProfileMerger } from '../../../src/llm-service/narrative/profile/profile-merger';
import { DEFAULT_NARRATIVE_CONFIG, type NarrativeConfig } from '../../../src/llm-service/narrative/types';

describe('ProfileMerger - 画像合成 (B3/B4/B6)', () => {
  let store: MemoryStore;
  let merger: ProfileMerger;
  const config: NarrativeConfig = {
    ...DEFAULT_NARRATIVE_CONFIG,
    patternSensitivity: 0.3,
    trajectoryAttributes: ['self_efficacy', 'emotional_expression', 'self_acceptance'],
  };

  beforeEach(() => {
    store = new MemoryStore();
    merger = new ProfileMerger(store, config);
  });

  afterEach(() => {
    store.close();
  });

  it('B3: 仅筛选 confidence >= 0.4 的假设', () => {
    // 高置信度假设
    store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'self_efficacy',
      hypothesis: '用户自信度较高', evidence: '多次主动表达观点',
      confidence: 0.8, observationRefs: [1], createdAt: Date.now(),
    });
    // 低置信度假设 — 应被排除
    store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'emotional_expression',
      hypothesis: '可能情绪表达不畅', evidence: '偶有沉默',
      confidence: 0.3, observationRefs: [2], createdAt: Date.now(),
    });

    const profile = merger.merge('default');
    expect(profile.hypotheses.length).toBe(1);
    expect(profile.hypotheses[0].domain).toBe('self_efficacy');
    expect(profile.hypotheses[0].confidence).toBe(0.8);
  });

  it('B6: 画像应携带 version 时间戳', () => {
    store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'self_efficacy',
      hypothesis: 'test', evidence: '', confidence: 0.8,
      observationRefs: [], createdAt: Date.now(),
    });

    const profile = merger.merge('default');
    expect(profile.version).toBeTruthy();
    // version 应该是 ISO 格式替换了冒号的字符串
    expect(profile.version.length).toBeGreaterThan(10);
    expect(profile.mergedAt).toBeGreaterThan(0);
  });

  it('B4: 画像应包含用户标识且不可用于在线决策', () => {
    store.insertHypothesis({
      userId: 'user123', version: 'v1', domain: 'self_efficacy',
      hypothesis: 'test', evidence: '', confidence: 0.8,
      observationRefs: [], createdAt: Date.now(),
    });

    const profile = merger.merge('user123');
    expect(profile.userId).toBe('user123');
    // B4 通过 JSDoc 标记约束，调用方必须了解
  });

  it('没有假设时返回空列表画像', () => {
    const profile = merger.merge('empty_user');
    expect(profile.userId).toBe('empty_user');
    expect(profile.hypotheses).toEqual([]);
    expect(profile.trajectoryAttributes).toBeDefined();
  });

  it('应包含轨迹属性（配置中的默认属性）', () => {
    const profile = merger.merge('default');
    expect(profile.trajectoryAttributes).toBeDefined();
    expect(Object.keys(profile.trajectoryAttributes)).toContain('self_efficacy');
    expect(Object.keys(profile.trajectoryAttributes)).toContain('emotional_expression');
    expect(Object.keys(profile.trajectoryAttributes)).toContain('self_acceptance');
  });

  it('应计算属性趋势方向', () => {
    // 写入一些量化指标数据
    const now = Date.now();
    // 写一些较旧的值（小值）
    for (let i = 0; i < 5; i++) {
      store.insertMetric({
        userId: 'default', metricType: 'emotional_trajectory_ma',
        metricName: 'self_efficacy_avg', value: 0.3,
        windowSize: 1, periodStart: now - 100000, periodEnd: now - 90000, createdAt: now - 90000,
      });
    }
    // 写一些较新的值（大值）
    for (let i = 0; i < 5; i++) {
      store.insertMetric({
        userId: 'default', metricType: 'emotional_trajectory_ma',
        metricName: 'self_efficacy_avg', value: 0.7,
        windowSize: 1, periodStart: now - 10000, periodEnd: now, createdAt: now,
      });
    }

    const profile = merger.merge('default');
    // 由于 sensitivity 为 0.3，0.7 - 0.3 = 0.4 > 0.3，应为 rising
    expect(profile.attributeTrends.self_efficacy).toBe('rising');
  });
});
