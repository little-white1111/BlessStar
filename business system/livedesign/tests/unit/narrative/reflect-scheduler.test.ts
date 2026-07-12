/**
 * L3 反思调度器单元测试
 *
 * 架构不变量验证：
 *   B3 — 反思输出必须标注 confidence，低于 0.4 的不写入画像
 *   B6 — 反思每轮输出必须携带 version 时间戳，支持多轮回溯
 */

import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { MemoryStore } from '../../../src/llm-service/narrative/storage/sqlite-store';
import { ReflectScheduler } from '../../../src/llm-service/narrative/reflect/reflect-scheduler';
import { buildInsightPrompt, buildCompactInsightPrompt } from '../../../src/llm-service/narrative/reflect/insight-prompt';
import { DEFAULT_NARRATIVE_CONFIG } from '../../../src/llm-service/narrative/types';
import type { ReflectiveHypothesis } from '../../../src/llm-service/narrative/types';

describe('ReflectScheduler - 数据不足跳过 (B3)', () => {
  let store: MemoryStore;
  let scheduler: ReflectScheduler;

  beforeEach(() => {
    store = new MemoryStore();
    scheduler = new ReflectScheduler(store, DEFAULT_NARRATIVE_CONFIG);
  });

  afterEach(() => {
    scheduler.stop();
    store.close();
  });

  it('B3: 观察数不足 minObservations 时应跳过', async () => {
    const config = { ...DEFAULT_NARRATIVE_CONFIG, minObservations: 10 };
    const s = new ReflectScheduler(store, config);

    // 只插入 3 条观察，不足 10
    for (let i = 0; i < 3; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
      });
    }

    const result = await s.run('default');
    expect(result).toEqual([]);
  });

  it('B3: 观察数足够时应调用回调生成假设', async () => {
    for (let i = 0; i < DEFAULT_NARRATIVE_CONFIG.minObservations; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
      });
    }

    const mockCallback = vi.fn().mockResolvedValue([
      {
        userId: 'default', version: 'test-v1', domain: 'self_efficacy',
        hypothesis: '用户表现出自信', evidence: '多次主动表达观点',
        confidence: 0.8, observationRefs: [1, 2, 3], createdAt: Date.now(),
      } as ReflectiveHypothesis,
    ]);

    scheduler.setReflectCallback(mockCallback);
    const result = await scheduler.run('default');
    expect(result.length).toBe(1);
    expect(result[0].domain).toBe('self_efficacy');
    expect(result[0].confidence).toBe(0.8);
    expect(mockCallback).toHaveBeenCalledTimes(1);
  });

  it('B6: 每轮反思应携带 version 时间戳', async () => {
    for (let i = 0; i < DEFAULT_NARRATIVE_CONFIG.minObservations; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
      });
    }

    const mockCallback = vi.fn().mockResolvedValue([
      {
        userId: 'default', version: 'v-round-1', domain: 'self_efficacy',
        hypothesis: '假设', evidence: '', confidence: 0.7,
        observationRefs: [], createdAt: Date.now(),
      } as ReflectiveHypothesis,
    ]);
    scheduler.setReflectCallback(mockCallback);

    // 第一轮
    await scheduler.run('default');
    // 等待以确保第二轮生成不同的版本时间戳
    await new Promise(resolve => setTimeout(resolve, 5));
    // 第二轮
    await scheduler.run('default');

    // 验证存储中有两个不同版本的假设
    const all = store.getHypothesesByConfidence('default', 0);
    const versions = [...new Set(all.map(h => h.version))];
    expect(versions.length).toBe(2);
  });

  it('B3: 回调返回空数组应正确处理', async () => {
    for (let i = 0; i < DEFAULT_NARRATIVE_CONFIG.minObservations; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
      });
    }

    scheduler.setReflectCallback(vi.fn().mockResolvedValue([]));
    const result = await scheduler.run('default');
    expect(result).toEqual([]);
  });

  it('B3: 无回调时直接返回空数组', async () => {
    for (let i = 0; i < DEFAULT_NARRATIVE_CONFIG.minObservations; i++) {
      store.insertObservation({
        sessionId: 's1', userId: 'default', content: `obs-${i}`, emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5, significant: false,
      });
    }

    const result = await scheduler.run('default');
    expect(result).toEqual([]);
  });
});

describe('insight-prompt 模板', () => {
  it('buildInsightPrompt 应包含 B3 约束说明', () => {
    const prompt = buildInsightPrompt({
      recentObservations: '[0] 用户说很开心',
      trajectoryAttributes: ['self_efficacy'],
    });
    expect(prompt).toContain('confidence');
    expect(prompt).toContain('0.4');
    expect(prompt).toContain('self_efficacy');
  });

  it('buildInsightPrompt 应包含历史假设', () => {
    const prompt = buildInsightPrompt({
      recentObservations: '[0] 用户说很开心',
      trajectoryAttributes: ['self_efficacy'],
      previousHypotheses: '- [self_efficacy] 用户自信 (confidence: 0.7)',
    });
    expect(prompt).toContain('之前的分析假设');
    expect(prompt).toContain('用户自信');
  });

  it('buildCompactInsightPrompt 应返回精简提示词', () => {
    const prompt = buildCompactInsightPrompt();
    expect(prompt).toContain('confidence');
    expect(prompt).toContain('0.4');
    expect(prompt).toContain('JSON');
  });
});
