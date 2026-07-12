/**
 * 叙事系统存储层单元测试（使用 MemoryStore）
 *
 * 架构不变量验证：
 *   B1 — L1 原始观察写入
 *   B5 — 遗忘权（用户数据完整删除）
 *   B6 — version 时间戳多轮回溯
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { MemoryStore } from '../../../src/llm-service/narrative/storage/sqlite-store';

describe('MemoryStore - L1 原始观察 (B1)', () => {
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
  });

  afterEach(() => {
    store.close();
  });

  it('应写入 L1 观察记录并返回自增 ID', () => {
    const id = store.insertObservation({
      sessionId: 'session-1',
      userId: 'default',
      content: '用户表达了积极情绪',
      emotion: 'happy',
      intensity: 0.8,
      valence: 0.9,
      arousal: 0.7,
      dominance: 0.6,
      significant: true,
    });
    expect(id).toBeGreaterThan(0);
  });

  it('应查询最近的观察记录（最新在最前）', () => {
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: '第一条', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
    });
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: '第二条', emotion: 'happy',
      intensity: 0.8, valence: 0.9, arousal: 0.7, dominance: 0.6, significant: true,
    });

    const recent = store.getRecentObservations('default', 10);
    expect(recent.length).toBe(2);
    expect(recent[0].content).toBe('第二条'); // 最新一条排在最前面
    expect(recent[0].significant).toBe(true);
    expect(recent[1].content).toBe('第一条');
    expect(recent[1].significant).toBe(false);
  });

  it('应按时间范围查询观察记录', () => {
    const now = Date.now();
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: '旧的', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
    });
    const results = store.getObservationsByTimeRange('default', now - 1000, now + 5000);
    expect(results.length).toBe(1);
    expect(results[0].content).toBe('旧的');
  });

  it('应正确统计观察总数', () => {
    for (let i = 0; i < 5; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
      });
    }
    expect(store.countObservations('default')).toBe(5);
    expect(store.countObservations('other')).toBe(0);
  });
});

describe('MemoryStore - L2 量化指标 CRUD', () => {
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
  });

  afterEach(() => {
    store.close();
  });

  it('应写入和查询量化指标', () => {
    store.insertMetric({
      userId: 'default', metricType: 'emotional_trajectory_ma', metricName: 'valence_avg',
      value: 0.65, windowSize: 10, periodStart: 0, periodEnd: 1000, createdAt: Date.now(),
    });
    const metrics = store.getRecentMetrics('default', 'emotional_trajectory_ma');
    expect(metrics.length).toBe(1);
    expect(metrics[0].metricName).toBe('valence_avg');
    expect(metrics[0].value).toBeCloseTo(0.65);
  });

  it('空用户应返回空列表', () => {
    const metrics = store.getRecentMetrics('nonexistent');
    expect(metrics.length).toBe(0);
  });
});

describe('MemoryStore - L3 反思假设 (B3/B6)', () => {
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
  });

  afterEach(() => {
    store.close();
  });

  it('应插入反思假设并携带 version', () => {
    const id = store.insertHypothesis({
      userId: 'default', version: '2025-06-01T00-00-00-000Z',
      domain: 'self_efficacy', hypothesis: '用户自信度提升', evidence: '多次积极表达',
      confidence: 0.8, observationRefs: [1, 2, 3], createdAt: Date.now(),
    });
    expect(id).toBeGreaterThan(0);
  });

  it('B6: 应按版本查询假设（多轮回溯）', () => {
    const v1 = '2025-06-01T00-00-00-000Z';
    const v2 = '2025-06-08T00-00-00-000Z';

    store.insertHypothesis({
      userId: 'default', version: v1, domain: 'self_efficacy',
      hypothesis: 'v1假设', evidence: '证据1', confidence: 0.7,
      observationRefs: [1], createdAt: Date.now(),
    });
    store.insertHypothesis({
      userId: 'default', version: v2, domain: 'self_efficacy',
      hypothesis: 'v2假设', evidence: '证据2', confidence: 0.8,
      observationRefs: [2, 3], createdAt: Date.now(),
    });

    const v1Results = store.getHypothesesByVersion('default', v1);
    expect(v1Results.length).toBe(1);
    expect(v1Results[0].hypothesis).toBe('v1假设');

    const v2Results = store.getHypothesesByVersion('default', v2);
    expect(v2Results.length).toBe(1);
    expect(v2Results[0].hypothesis).toBe('v2假设');
  });

  it('B6: 应返回所有版本列表', () => {
    store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'test',
      hypothesis: 'h1', evidence: '', confidence: 0.5,
      observationRefs: [], createdAt: Date.now(),
    });
    store.insertHypothesis({
      userId: 'default', version: 'v2', domain: 'test',
      hypothesis: 'h2', evidence: '', confidence: 0.6,
      observationRefs: [], createdAt: Date.now(),
    });
    const versions = store.getAllVersions('default');
    expect(versions.length).toBe(2);
  });

  it('B3: 应按置信度筛选假设', () => {
    store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'test',
      hypothesis: '高置信度', evidence: '', confidence: 0.9,
      observationRefs: [], createdAt: Date.now(),
    });
    store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'test',
      hypothesis: '低置信度', evidence: '', confidence: 0.3,
      observationRefs: [], createdAt: Date.now(),
    });

    const high = store.getHypothesesByConfidence('default', 0.7);
    expect(high.length).toBe(1);
    expect(high[0].hypothesis).toBe('高置信度');

    const all = store.getHypothesesByConfidence('default', 0);
    expect(all.length).toBe(2);
  });
});

describe('MemoryStore - L4 动态画像', () => {
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
  });

  afterEach(() => {
    store.close();
  });

  it('应保存和获取最新画像', () => {
    const profile = {
      version: 'v1', userId: 'default', hypotheses: [],
      trajectoryAttributes: { self_efficacy: [0.5] },
      mergedAt: Date.now(), attributeTrends: { self_efficacy: 'stable' as const },
    };
    store.saveProfileSnapshot(profile);

    const loaded = store.getLatestProfile('default');
    expect(loaded).not.toBeNull();
    expect(loaded!.version).toBe('v1');
    expect(loaded!.trajectoryAttributes.self_efficacy).toEqual([0.5]);
  });

  it('无画像时返回 null', () => {
    const loaded = store.getLatestProfile('nonexistent');
    expect(loaded).toBeNull();
  });
});

describe('MemoryStore - B5: 遗忘权', () => {
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
  });

  afterEach(() => {
    store.close();
  });

  it('B5: 删除用户数据后所有数据清空', () => {
    store.insertObservation({
      sessionId: 's1', userId: 'user1', content: 'test', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
    });
    store.insertMetric({
      userId: 'user1', metricType: 'emotional_trajectory_ma', metricName: 'test',
      value: 0.5, windowSize: 0, periodStart: 0, periodEnd: 0, createdAt: Date.now(),
    });
    store.insertHypothesis({
      userId: 'user1', version: 'v1', domain: 'test',
      hypothesis: 'test', evidence: '', confidence: 0.5,
      observationRefs: [], createdAt: Date.now(),
    });
    store.saveProfileSnapshot({
      version: 'v1', userId: 'user1', hypotheses: [],
      trajectoryAttributes: {}, mergedAt: Date.now(), attributeTrends: {},
    });

    expect(store.countObservations('user1')).toBe(1);
    expect(store.getHypothesesByConfidence('user1', 0).length).toBe(1);

    store.deleteUserData('user1');

    expect(store.countObservations('user1')).toBe(0);
    expect(store.getHypothesesByConfidence('user1', 0).length).toBe(0);
    expect(store.getRecentMetrics('user1').length).toBe(0);
    expect(store.getLatestProfile('user1')).toBeNull();

    store.insertObservation({
      sessionId: 's1', userId: 'user2', content: 'other', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
    });
    expect(store.countObservations('user2')).toBe(1);
  });
});

describe('MemoryStore - 清理与统计', () => {
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
  });

  afterEach(() => {
    store.close();
  });

  it('应清理过期观察记录', () => {
    const oldTime = Date.now() - 1000 * 24 * 60 * 60 * 1000;
    // 手动创建一条旧记录
    const obs = {
      id: undefined as unknown as number,
      sessionId: 's1', userId: 'default', content: 'old', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5,
      significant: false, createdAt: oldTime,
    };
    (store as any).observations.push(obs);

    expect(store.countObservations('default')).toBe(1);

    const deleted = store.cleanOldObservations(365);
    expect(deleted).toBe(1);
    expect(store.countObservations('default')).toBe(0);
  });

  it('应返回正确的统计信息', () => {
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: 'test', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
    });
    const stats = store.getStats();
    expect(stats.totalObservations).toBe(1);
    expect(stats.totalMetrics).toBe(0);
    expect(stats.totalHypotheses).toBe(0);
    expect(stats.totalSnapshots).toBe(0);
  });
});
