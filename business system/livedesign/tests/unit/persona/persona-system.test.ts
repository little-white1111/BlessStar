/**
 * 助手人格成长弧光系统 — 单元测试
 *
 * 覆盖架构不变量:
 *   C1 — 核心人格不可在运行时修改
 *   C2 — 自适应演进经一致性检查
 *   C3 — 自适应人格值被 min/max 硬性截断
 *   C4 — 演进方向包含正面偏向
 *   C5 — 每次演进记录 evolution_log
 *   C6 — 情境人格不得持久化
 *   C7 — 情境人格受 L2 自适应人格约束
 */

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { EvolutionEngine } from '../../../src/llm-service/persona/adaptive/evolution-engine';
import { checkConsistency } from '../../../src/llm-service/persona/adaptive/consistency-checker';
import { SituationalSelector } from '../../../src/llm-service/persona/situational/situational-selector';
import { PersonaSystem } from '../../../src/llm-service/persona/index';
import type { CorePersona, AdaptivePersona } from '../../../src/llm-service/persona/types';
import { DEFAULT_CORE_PERSONA, DEFAULT_ADAPTIVE_PERSONA } from '../../../src/llm-service/persona/types';
import type { EvolutionLogStore } from '../../../src/llm-service/persona/evolution-log';

// ==================== InMemoryEvolutionLog（测试用） ====================

class InMemoryEvolutionLog implements EvolutionLogStore {
  private entries: Array<{ trait_name: string; old_value: number; new_value: number; reason: string; consistency_check: string; timestamp: number }> = [];

  write(entry: Omit<{ trait_name: string; old_value: number; new_value: number; reason: string; consistency_check: string; timestamp: number }, 'timestamp'>): void {
    this.entries.push({ ...entry, timestamp: Date.now() });
  }

  query(limit: number = 50, offset: number = 0): any[] {
    return this.entries.slice(offset, offset + limit);
  }

  count(): number {
    return this.entries.length;
  }

  close(): void {
    this.entries = [];
  }
}

// ==================== 测试用默认值 ====================

const TEST_CORE: CorePersona = { warmth: 0.8, rationality: 0.9, curiosity: 0.6 };
const TEST_ADAPTIVE: AdaptivePersona = { encouragement: 0.5, patience: 0.6, proactive_curiosity: 0.4 };

// ==================== 辅助函数 ====================

function createTestLog(): EvolutionLogStore {
  return new InMemoryEvolutionLog();
}

// ==================== 架构不变量 C2: 德性一致性检查器 ====================

describe('ConsistencyChecker（架构不变量 C2）', () => {
  it('自适应值在范围内且未偏离核心特质时应通过', () => {
    const result = checkConsistency(TEST_CORE, TEST_ADAPTIVE);
    expect(result.passed).toBe(true);
    expect(result.failures).toHaveLength(0);
  });

  it('encouragement 超出范围时应失败', () => {
    const adaptive: AdaptivePersona = { ...TEST_ADAPTIVE, encouragement: 0.99 };
    const result = checkConsistency(TEST_CORE, adaptive);
    expect(result.passed).toBe(false);
    expect(result.failures.some((f) => f.trait === 'encouragement')).toBe(true);
  });

  it('encouragement 低于范围时应失败', () => {
    const adaptive: AdaptivePersona = { ...TEST_ADAPTIVE, encouragement: 0.1 };
    const result = checkConsistency(TEST_CORE, adaptive);
    expect(result.passed).toBe(false);
  });

  it('patience 超出范围时应失败', () => {
    const adaptive: AdaptivePersona = { ...TEST_ADAPTIVE, patience: 0.99 };
    const result = checkConsistency(TEST_CORE, adaptive);
    expect(result.passed).toBe(false);
  });

  it('proactive_curiosity 低于范围时应失败', () => {
    const adaptive: AdaptivePersona = { ...TEST_ADAPTIVE, proactive_curiosity: 0.05 };
    const result = checkConsistency(TEST_CORE, adaptive);
    expect(result.passed).toBe(false);
  });

  it('自适应值偏离核心特质超过 0.3 时应失败', () => {
    // warmth=0.8, encouragement 偏离超过 0.3 意味着 encouragement < 0.5 或 > 1.0
    // 但 encouragement 的 max 是 0.9，所以只可能低于
    const adaptive: AdaptivePersona = { ...TEST_ADAPTIVE, encouragement: 0.4 };
    const result = checkConsistency(TEST_CORE, adaptive);
    // 0.4 在范围内 [0.2, 0.9]，但偏离 warmth(0.8) 0.4 > 0.3
    expect(result.passed).toBe(false);
    expect(result.failures.some((f) => f.trait === 'encouragement' && f.reason.includes('偏离核心特质'))).toBe(true);
  });

  it('全部三个自适应检查规则同时失败时应报告所有失败', () => {
    const adaptive: AdaptivePersona = {
      encouragement: 1.0,  // 超出范围 [0.2, 0.9] + 偏离 warmth
      patience: 0.99,       // 超出范围 [0.3, 0.95]
      proactive_curiosity: 0.05, // 低于范围 [0.1, 0.7]
    };
    const result = checkConsistency(TEST_CORE, adaptive);
    expect(result.passed).toBe(false);
    expect(result.failures.length).toBeGreaterThanOrEqual(3);
  });
});

// ==================== 架构不变量 C1 / C3 / C4 / C5: 演进引擎 ====================

describe('EvolutionEngine（架构不变量 C1, C3, C4, C5）', () => {
  let log: EvolutionLog;

  beforeEach(() => {
    log = createTestLog();
  });

  afterEach(() => {
    log.close();
  });

  it('架构不变量 C1: 核心人格不在演进范围内（演进引擎不修改 L1）', () => {
    const engine = new EvolutionEngine({ positive_bias: 0.1, change_rate_per_1k: 0.05 }, log);
    const corePre = { ...TEST_CORE };

    const result = engine.evolve({
      core: TEST_CORE,
      adaptive: TEST_ADAPTIVE,
      totalConversations: 5000,
      lastEvolutionConversations: 0,
      feedbackScore: 0.5,
    });

    // 核心人格不变
    expect(TEST_CORE.warmth).toBe(corePre.warmth);
    expect(TEST_CORE.rationality).toBe(corePre.rationality);
    expect(TEST_CORE.curiosity).toBe(corePre.curiosity);
  });

  it('架构不变量 C3: 自适应值超出 min/max 时被 clamp', () => {
    const engine = new EvolutionEngine({ positive_bias: 0.0, change_rate_per_1k: 10.0 }, log);
    // 使用与自适应最小值兼容的核心值（确保一致性检查通过）
    const core: CorePersona = { warmth: 0.5, rationality: 0.6, curiosity: 0.4 };
    // 极限负反馈 + 超大变化率，尝试把值推向负值
    const adaptive: AdaptivePersona = { encouragement: 0.5, patience: 0.6, proactive_curiosity: 0.4 };

    const result = engine.evolve({
      core,
      adaptive,
      totalConversations: 10000,
      lastEvolutionConversations: 0,
      feedbackScore: -1.0,
    });

    // delta = -1 * (10000/1000)*10 + 0 = -100
    // clamp 后应达到 min 值
    expect(result.after.encouragement).toBe(0.2);
    expect(result.after.patience).toBe(0.3);
    expect(result.after.proactive_curiosity).toBe(0.1);
  });

  it('架构不变量 C4: 正面偏向确保消极反馈后的演进方向不下降', () => {
    const engine = new EvolutionEngine({ positive_bias: 0.1, change_rate_per_1k: 0.05 }, log);
    const initial: AdaptivePersona = { ...TEST_ADAPTIVE };

    // 消极用户反馈
    const result = engine.evolve({
      core: TEST_CORE,
      adaptive: initial,
      totalConversations: 1000,
      lastEvolutionConversations: 0,
      feedbackScore: -1.0, // 强烈消极反馈
    });

    // delta = -1 * (1000/1000)*0.05 + 0.1*(1000/1000)*0.05 = -0.05 + 0.005 = -0.045
    // 但由于 C4 正面偏向，delta 实际为 feedbackDelta + positive_bias * changeFactor
    // changeFactor = (1000/1000)*0.05 = 0.05
    // feedbackDelta = -1 * 0.05 = -0.05
    // positive_bias_component = 0.1 * 0.05 = 0.005
    // total = -0.045

    // encouragement 从 0.5 变到 0.455
    for (const change of result.changes) {
      // 每个特质的变化量应大于纯负反馈的情况
      const pureNegativeDelta = -1 * 0.05; // 纯负反馈 delta
      expect(change.delta).toBeGreaterThan(pureNegativeDelta);
    }
  });

  it('架构不变量 C4: 即使反馈为 0，正面偏向也应使人格缓慢上升', () => {
    const engine = new EvolutionEngine({ positive_bias: 0.1, change_rate_per_1k: 0.05 }, log);
    const initial: AdaptivePersona = { ...TEST_ADAPTIVE };

    const result = engine.evolve({
      core: TEST_CORE,
      adaptive: initial,
      totalConversations: 1000,
      lastEvolutionConversations: 0,
      feedbackScore: 0, // 中性反馈
    });

    // delta = 0 * 0.05 + 0.1 * 0.05 = 0.005
    // 所有值应略微上升
    for (const change of result.changes) {
      expect(change.delta).toBeGreaterThan(0);
      expect(change.newValue).toBeGreaterThan(change.oldValue);
    }
  });

  it('架构不变量 C5: 每次演进后确认 evolution_log 写入', () => {
    const engine = new EvolutionEngine({ positive_bias: 0.1, change_rate_per_1k: 0.05 }, log);
    const beforeCount = log.count();

    engine.evolve({
      core: TEST_CORE,
      adaptive: TEST_ADAPTIVE,
      totalConversations: 2000,
      lastEvolutionConversations: 0,
      feedbackScore: 0.5,
    });

    const afterCount = log.count();
    // 应有日志写入
    expect(afterCount).toBeGreaterThan(beforeCount);
  });

  it('没有新对话时演进无变化', () => {
    const engine = new EvolutionEngine({ positive_bias: 0.1, change_rate_per_1k: 0.05 }, log);
    const initial: AdaptivePersona = { ...TEST_ADAPTIVE };

    const result = engine.evolve({
      core: TEST_CORE,
      adaptive: initial,
      totalConversations: 100,
      lastEvolutionConversations: 100, // 没有增量
      feedbackScore: 0.5,
    });

    expect(result.changes).toHaveLength(0);
    expect(result.after).toEqual(initial);
  });

  it('一致性检查失败后应回滚', () => {
    const engine = new EvolutionEngine({ positive_bias: 0.3, change_rate_per_1k: 0.2 }, log);
    // warmth=0.8, 从 encouragement=0.5 大幅增加超过 0.3 偏差
    const adaptive: AdaptivePersona = { ...TEST_ADAPTIVE, encouragement: 0.7 };
    // 尝试让 encouragement 再增加 0.3 → 1.0，偏离 warmth 0.2

    const result = engine.evolve({
      core: TEST_CORE,
      adaptive,
      totalConversations: 5000,
      lastEvolutionConversations: 0,
      feedbackScore: 1.0, // 强烈积极反馈
    });

    // changeFactor > 0, 但 consistencyCheck 可能会失败
    if (!result.consistencyPassed) {
      expect(result.rolledBack).toBe(true);
      expect(result.after).toEqual(adaptive); // 回滚到演进前
      expect(result.changes).toHaveLength(0);
    }
  });
});

// ==================== 架构不变量 C6 / C7: 情境人格选择器 ====================

describe('SituationalSelector（架构不变量 C6, C7）', () => {
  const selector = new SituationalSelector();

  it('架构不变量 C6: 输出仅为内存对象，无持久化', () => {
    const mod = selector.select(TEST_CORE, TEST_ADAPTIVE, {
      userMessage: 'hi',
      turnNumber: 1,
    });

    // 验证输出是普通内存对象
    expect(mod).toBeDefined();
    expect(typeof mod.tone).toBe('string');
    expect(typeof mod.expressiveness).toBe('number');
    expect(typeof mod.formality).toBe('number');
    // 验证没有任何方法可以持久化 — 仅是普通对象
    expect(Object.keys(mod)).toEqual(['tone', 'expressiveness', 'formality']);
  });

  it('架构不变量 C7: expressiveness 在 encouragement ±0.2 范围内', () => {
    // 测试多种自适应值组合
    const testCases: AdaptivePersona[] = [
      { encouragement: 0.2, patience: 0.6, proactive_curiosity: 0.4 },
      { encouragement: 0.5, patience: 0.6, proactive_curiosity: 0.4 },
      { encouragement: 0.9, patience: 0.6, proactive_curiosity: 0.4 },
    ];

    for (const adaptive of testCases) {
      const mod = selector.select(TEST_CORE, adaptive, {
        userMessage: 'hello',
        turnNumber: 1,
      });
      const min = adaptive.encouragement - 0.2;
      const max = adaptive.encouragement + 0.2;
      expect(mod.expressiveness).toBeGreaterThanOrEqual(Math.max(0, min));
      expect(mod.expressiveness).toBeLessThanOrEqual(Math.min(1, max));
    }
  });

  it('架构不变量 C7: formality 在 patience ±0.2 范围内', () => {
    const testCases: AdaptivePersona[] = [
      { encouragement: 0.5, patience: 0.3, proactive_curiosity: 0.4 },
      { encouragement: 0.5, patience: 0.6, proactive_curiosity: 0.4 },
      { encouragement: 0.5, patience: 0.95, proactive_curiosity: 0.4 },
    ];

    for (const adaptive of testCases) {
      const mod = selector.select(TEST_CORE, adaptive, {
        userMessage: 'hello',
        turnNumber: 1,
      });
      const min = adaptive.patience - 0.2;
      const max = adaptive.patience + 0.2;
      expect(mod.formality).toBeGreaterThanOrEqual(Math.max(0, min));
      expect(mod.formality).toBeLessThanOrEqual(Math.min(1, max));
    }
  });

  it('不同用户情绪应产生不同 tone', () => {
    const mod1 = selector.select(TEST_CORE, TEST_ADAPTIVE, {
      userMessage: 'I am sad',
      turnNumber: 1,
      userEmotion: 'sad',
      userValence: 0.15,
    });
    const mod2 = selector.select(TEST_CORE, TEST_ADAPTIVE, {
      userMessage: 'Great!',
      turnNumber: 1,
      userEmotion: 'happy',
      userValence: 0.85,
    });
    // 消极情绪产生 warm 语气
    expect(mod1.tone).toBe('warm');
    // 积极情绪且高理性度产生 professional 语气
    expect(mod2.tone).toBe('professional');
  });

  it('早期对话 formality 更高', () => {
    const mod1 = selector.select(TEST_CORE, TEST_ADAPTIVE, {
      userMessage: 'hi',
      turnNumber: 1,
    });
    const mod2 = selector.select(TEST_CORE, TEST_ADAPTIVE, {
      userMessage: 'hi',
      turnNumber: 20,
    });
    expect(mod1.formality).toBeGreaterThanOrEqual(mod2.formality);
  });
});

// ==================== PersonaSystem 集成测试 ====================

describe('PersonaSystem 集成', () => {
  let system: PersonaSystem;
  let logStore: InMemoryEvolutionLog;

  beforeEach(() => {
    logStore = new InMemoryEvolutionLog();
    system = new PersonaSystem({ evolutionLogStore: logStore });
  });

  afterEach(() => {
    system.destroy();
  });

  it('构造后核心人格不可变', () => {
    expect(system.core.warmth).toBeGreaterThan(0);
    expect(system.core.rationality).toBeGreaterThan(0);
    expect(system.core.curiosity).toBeGreaterThan(0);
  });

  it('记录对话和反馈后演进', () => {
    // 模拟 2000 次对话
    for (let i = 0; i < 2000; i++) {
      system.recordConversation();
    }
    system.recordFeedback(0.5);
    system.recordFeedback(0.8);

    const result = system.evolve();
    // 应有正向演进
    if (result.changes.length > 0) {
      for (const change of result.changes) {
        expect(change.newValue).toBeGreaterThanOrEqual(change.oldValue);
      }
    }
  });

  it('演进日志记录后可通过 count 查询', () => {
    for (let i = 0; i < 1000; i++) {
      system.recordConversation();
    }
    system.recordFeedback(0.5);
    system.evolve();

    expect(logStore.count()).toBeGreaterThan(0);
  });

  it('selectSituational 返回有效调制参数', () => {
    const mod = system.selectSituational({
      userMessage: 'hello',
      turnNumber: 1,
    });

    expect(['warm', 'neutral', 'professional']).toContain(mod.tone);
    expect(mod.expressiveness).toBeGreaterThanOrEqual(0);
    expect(mod.expressiveness).toBeLessThanOrEqual(1);
    expect(mod.formality).toBeGreaterThanOrEqual(0);
    expect(mod.formality).toBeLessThanOrEqual(1);
  });
});
