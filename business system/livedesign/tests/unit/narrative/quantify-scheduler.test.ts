/**
 * L2 量化指标计算器单元测试
 *
 * 架构不变量 B2: L2 量化指标由程序自动计算，不得引入 LLM
 * 所有计算均为程序化算法（滑动平均/频率统计/峰值检测）
 */

import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { MemoryStore } from '../../../src/llm-service/narrative/storage/sqlite-store';
import { QuantifyScheduler } from '../../../src/llm-service/narrative/quantify/quantify-scheduler';
import { DEFAULT_NARRATIVE_CONFIG } from '../../../src/llm-service/narrative/types';

describe('QuantifyScheduler - 情绪轨迹滑动平均 (B2)', () => {
  let store: MemoryStore;
  let scheduler: QuantifyScheduler;

  beforeEach(() => {
    store = new MemoryStore();
    scheduler = new QuantifyScheduler(store, DEFAULT_NARRATIVE_CONFIG, 1000);
  });

  afterEach(() => {
    scheduler.stop();
    store.close();
  });

  it('B2: 无观察时返回默认值', () => {
    const ma = scheduler.calculateTrajectoryMA('default');
    expect(ma.valenceAvg).toBe(0.5);
    expect(ma.arousalAvg).toBe(0.5);
    expect(ma.dominanceAvg).toBe(0.5);
    expect(ma.windowSize).toBe(0);
  });

  it('B2: 应正确计算 VAD 滑动平均', () => {
    for (let i = 0; i < 3; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'happy',
        intensity: 0.8, valence: 0.9, arousal: 0.7, dominance: 0.6,
        significant: false,
      });
    }
    const ma = scheduler.calculateTrajectoryMA('default', 10);
    expect(ma.valenceAvg).toBeCloseTo(0.9);
    expect(ma.arousalAvg).toBeCloseTo(0.7);
    expect(ma.dominanceAvg).toBeCloseTo(0.6);
    expect(ma.windowSize).toBe(3);
  });

  it('B2: 应限制窗口大小', () => {
    for (let i = 0; i < 10; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: i / 10, arousal: 0.5, dominance: 0.5,
        significant: false,
      });
    }
    const ma = scheduler.calculateTrajectoryMA('default', 5);
    expect(ma.windowSize).toBeLessThanOrEqual(5);
  });
});

describe('QuantifyScheduler - 主题密度 (B2)', () => {
  let store: MemoryStore;
  let scheduler: QuantifyScheduler;

  beforeEach(() => {
    store = new MemoryStore();
    scheduler = new QuantifyScheduler(store, DEFAULT_NARRATIVE_CONFIG, 1000);
  });

  afterEach(() => {
    scheduler.stop();
    store.close();
  });

  it('B2: 应统计情绪标签频率', () => {
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: '开心', emotion: 'happy',
      intensity: 0.8, valence: 0.8, arousal: 0.7, dominance: 0.6, significant: false,
    });
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: '更开心', emotion: 'happy',
      intensity: 0.9, valence: 0.9, arousal: 0.8, dominance: 0.7, significant: false,
    });
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: '难过', emotion: 'sad',
      intensity: 0.3, valence: 0.2, arousal: 0.3, dominance: 0.4, significant: false,
    });

    const density = scheduler.calculateThemeDensity('default', 100);
    expect(density.total).toBe(3);
    expect(density.themes['happy']).toBe(2);
    expect(density.themes['sad']).toBe(1);
  });

  it('B2: 空数据返回空主题', () => {
    const density = scheduler.calculateThemeDensity('default');
    expect(density.total).toBe(0);
    expect(Object.keys(density.themes).length).toBe(0);
  });
});

describe('QuantifyScheduler - 互动模式 (B2)', () => {
  let store: MemoryStore;
  let scheduler: QuantifyScheduler;

  beforeEach(() => {
    store = new MemoryStore();
    scheduler = new QuantifyScheduler(store, DEFAULT_NARRATIVE_CONFIG, 1000);
  });

  afterEach(() => {
    scheduler.stop();
    store.close();
  });

  it('B2: 不足 2 条记录时返回默认值', () => {
    const pattern = scheduler.calculateInteractionPattern('default');
    expect(pattern.messagesPerDay).toBe(0);
    expect(pattern.avgInterval).toBe(0);
    expect(pattern.peakHour).toBe(0);
  });

  it('B2: 应正确计算互动模式', () => {
    const now = Date.now();
    const hourMs = 60 * 60 * 1000;

    // 从最旧到最新插入（MemoryStore 使用 unshift 将最新插入放到最前）
    // 确保观察按 created_at DESC 排序以便 calculateInteractionPattern 正确计算
    for (let i = 2; i >= 0; i--) {
      const id = store.insertObservation({
        sessionId: 's1', userId: 'default', content: `msg-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5,
        significant: false,
      });
      // 使用内部访问设置不同的 created_at
      const obs = (store as any).observations.find((o: any) => o.id === id);
      if (obs) obs.createdAt = now - i * hourMs;
    }

    const pattern = scheduler.calculateInteractionPattern('default', 200);
    expect(pattern.messagesPerDay).toBeGreaterThan(0);
    expect(pattern.avgInterval).toBeGreaterThan(0);
  });
});

describe('QuantifyScheduler - run() 定时执行 (B2)', () => {
  let store: MemoryStore;
  let scheduler: QuantifyScheduler;

  beforeEach(() => {
    store = new MemoryStore();
    scheduler = new QuantifyScheduler(store, DEFAULT_NARRATIVE_CONFIG, 1000);
  });

  afterEach(() => {
    scheduler.stop();
    store.close();
  });

  it('B2: run() 应生成量化指标写入存储', () => {
    for (let i = 0; i < 5; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5,
        significant: false,
      });
    }

    scheduler.run('default');

    const metrics = store.getRecentMetrics('default');
    const trajectoryNames = metrics.filter(m => m.metricType === 'emotional_trajectory_ma').map(m => m.metricName);
    expect(trajectoryNames).toContain('valence_avg');
    expect(trajectoryNames).toContain('arousal_avg');
    expect(trajectoryNames).toContain('dominance_avg');
  });

  it('B2: start() 应启动定时器并立即执行一次', () => {
    vi.useFakeTimers();
    store.insertObservation({
      sessionId: 's1', userId: 'default', content: 'test', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
    });

    scheduler.start('default');

    const metrics = store.getRecentMetrics('default');
    expect(metrics.length).toBeGreaterThan(0);

    scheduler.stop();
    vi.useRealTimers();
  });
});
