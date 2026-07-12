/**
 * 叙事系统集成测试（L1→L2→L3→L4 全链路）
 *
 * 架构不变量验证：
 *   B1 — L1 写入 → Expresser 回调
 *   B5 — 遗忘权删除
 *   B7 — 故障注入：叙事子系统异常不影响主管线
 */

import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { NarrativeSystem } from '../../../src/llm-service/narrative';
import { Expresser, type ExpressionResult } from '../../../src/llm-service/agent/expresser';
import { MemoryStore } from '../../../src/llm-service/narrative/storage/sqlite-store';
import { DEFAULT_NARRATIVE_CONFIG } from '../../../src/llm-service/narrative/types';

describe('NarrativeSystem - L1 写入 (B1)', () => {
  let system: NarrativeSystem;
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
    system = new NarrativeSystem(DEFAULT_NARRATIVE_CONFIG, undefined, store);
  });

  afterEach(() => {
    system.destroy();
  });

  it('B1: record() 应写入原始观察', () => {
    system.setContext('session-1', 'user1');
    system.record({
      sessionId: 'session-1', content: '用户感到开心',
      emotion: 'happy', intensity: 0.8,
      valence: 0.9, arousal: 0.7, dominance: 0.6,
    });

    const observations = system.store.getRecentObservations('user1', 10);
    expect(observations.length).toBe(1);
    expect(observations[0].content).toBe('用户感到开心');
    expect(observations[0].emotion).toBe('happy');
  });

  it('B1: recordFromExpression 应从 ExpressionResult 提取数据', () => {
    system.setContext('session-1');

    const result: ExpressionResult = {
      text: '我理解你的感受',
      emotionUpdate: {
        emotion: 'empathetic', action: 'nod', intensity: 0.7,
        valence: 0.8, arousal: 0.6, dominance: 0.7,
      },
    };

    system.recordFromExpression(result);

    const observations = system.store.getRecentObservations('default', 10);
    expect(observations.length).toBe(1);
    expect(observations[0].content).toBe('我理解你的感受');
    expect(observations[0].emotion).toBe('empathetic');
    expect(observations[0].intensity).toBe(0.7);
  });

  it('B1: 高强度情感应标记为 significant', () => {
    system.record({
      sessionId: 's1', content: '非常强烈的情绪表达',
      emotion: 'angry', intensity: 1.0,
      valence: 0.1, arousal: 0.9, dominance: 0.8,
    });

    system.record({
      sessionId: 's1', content: '平静的状态',
      emotion: 'calm', intensity: 0.5,
      valence: 0.5, arousal: 0.5, dominance: 0.5,
    });

    const observations = system.store.getRecentObservations('default', 10);
    // 最新记录（calm）在最前，应标记为非 significant
    // 之前的记录（angry, intensity 1.0）在后面，应标记为 significant
    expect(observations[0].significant).toBe(false);
    expect(observations[1].significant).toBe(true);
  });
});

describe('NarrativeSystem - L4 画像合成 (B3/B4/B6)', () => {
  let system: NarrativeSystem;
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
    system = new NarrativeSystem(DEFAULT_NARRATIVE_CONFIG, undefined, store);
  });

  afterEach(() => {
    system.destroy();
  });

  it('B3: mergeProfile 应过滤低置信度假设', () => {
    system.store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'self_efficacy',
      hypothesis: '高置信度', evidence: '', confidence: 0.9,
      observationRefs: [], createdAt: Date.now(),
    });
    system.store.insertHypothesis({
      userId: 'default', version: 'v1', domain: 'emotional_expression',
      hypothesis: '低置信度', evidence: '', confidence: 0.3,
      observationRefs: [], createdAt: Date.now(),
    });

    const profile = system.mergeProfile('default');
    expect(profile).not.toBeNull();
    expect(profile!.hypotheses.length).toBe(1);
    expect(profile!.hypotheses[0].confidence).toBeGreaterThanOrEqual(0.4);
  });

  it('B6: 画像应包含 version', () => {
    const profile = system.mergeProfile('default');
    expect(profile!.version).toBeTruthy();
  });

  it('B4: getProfile 应返回最新的画像', () => {
    system.mergeProfile('default');
    const profile = system.getProfile('default');
    expect(profile).not.toBeNull();
    expect(profile!.userId).toBe('default');
  });
});

describe('NarrativeSystem - B5: 遗忘权', () => {
  let system: NarrativeSystem;
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
    system = new NarrativeSystem(DEFAULT_NARRATIVE_CONFIG, undefined, store);
  });

  afterEach(() => {
    system.destroy();
  });

  it('B5: deleteUserData 应清空用户数据', () => {
    system.record({
      sessionId: 's1', content: 'test', emotion: 'neutral',
      intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5,
    });

    expect(system.store.countObservations('default')).toBe(1);

    system.deleteUserData('default');
    expect(system.store.countObservations('default')).toBe(0);
  });
});

describe('NarrativeSystem - B7: 故障隔离', () => {
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
  });

  afterEach(() => {
    // noop
  });

  it('B7: Expresser 的 narrativeHook 抛出异常不应影响主管线', () => {
    const expresser = new Expresser();

    // 注册一个会抛出的 hook
    expresser.setNarrativeHook(() => {
      throw new Error('叙事系统异常');
    });

    // express() 应正常返回结果，不抛出异常
    const mockInference = {
      text: '正常响应',
      emotionUpdate: { emotion: 'happy', action: 'smile', intensity: 0.7 },
    } as any;
    const mockVAD = { valence: 0.8, arousal: 0.6, dominance: 0.7 };

    expect(() => {
      const result = expresser.express(mockInference, mockVAD);
      expect(result.text).toBe('正常响应');
    }).not.toThrow();
  });

  it('B7: NarrativeSystem record() 在 store 关闭后不应抛出', () => {
    const system = new NarrativeSystem(DEFAULT_NARRATIVE_CONFIG, undefined, store);

    // 销毁系统关闭 store，再调用 record 应被 try-catch 捕获
    system.destroy();

    // B7: 即使 store 已关闭，record 也不应抛出
    expect(() => {
      system.record({
        sessionId: 's1', content: 'test', emotion: 'neutral',
        intensity: 0.5, valence: 0.5, arousal: 0.5, dominance: 0.5,
      });
    }).not.toThrow();
  });

  it('B7: 启动失败不抛出', () => {
    const system = new NarrativeSystem(DEFAULT_NARRATIVE_CONFIG, undefined, store);
    // 多次 start 应安全
    system.start();
    system.start(); // 重复 start 不报错
    system.stop();
    system.destroy();
  });
});

describe('NarrativeSystem - Expresser 集成 (B1/B7)', () => {
  let system: NarrativeSystem;
  let expresser: Expresser;
  let store: MemoryStore;

  beforeEach(() => {
    store = new MemoryStore();
    system = new NarrativeSystem(DEFAULT_NARRATIVE_CONFIG, undefined, store);
    expresser = new Expresser();
    expresser.setNarrativeHook(
      (result) => system.recordFromExpression(result),
    );
  });

  afterEach(() => {
    system.destroy();
  });

  it('B1/B7: Expresser 调用后观察应写入 L1', () => {
    const mockInference = {
      text: '我理解你的感受',
      emotionUpdate: { emotion: 'empathetic', action: 'nod', intensity: 0.7 },
    } as any;
    const mockVAD = { valence: 0.8, arousal: 0.6, dominance: 0.7 };

    expresser.express(mockInference, mockVAD);

    const observations = system.store.getRecentObservations('default', 10);
    expect(observations.length).toBe(1);
    expect(observations[0].content).toBe('我理解你的感受');
  });

  it('B1/B7: 多次 express 应记录多条观察', () => {
    const inference1 = {
      text: '第一次', emotionUpdate: { emotion: 'happy', action: 'smile', intensity: 0.7 },
    } as any;
    const inference2 = {
      text: '第二次', emotionUpdate: { emotion: 'sad', action: 'frown', intensity: 0.3 },
    } as any;
    const vad = { valence: 0.5, arousal: 0.5, dominance: 0.5 };

    expresser.express(inference1, vad);
    expresser.express(inference2, vad);

    const observations = system.store.getRecentObservations('default', 10);
    expect(observations.length).toBe(2);
  });
});
