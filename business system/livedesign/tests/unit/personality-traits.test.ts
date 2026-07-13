/**
 * 人格特质单元测试
 * 测试 constrainDelta 的特质约束逻辑
 */
import { describe, it, expect } from 'vitest';
import { constrainDelta, DEFAULT_TRAITS } from '../../src/llm-service/personality/traits';
import type { AffectiveState } from '../../src/llm-service/personality/affective';

describe('constrainDelta', () => {
  it('默认特质应放大 valence 变化（共情度 0.7）', () => {
    const delta: AffectiveState = { valence: 0.3, arousal: 0.0, dominance: 0.0 };
    const result = constrainDelta(delta, DEFAULT_TRAITS);
    // 0.3 * (0.5 + 0.7) = 0.3 * 1.2 = 0.36
    expect(result.valence).toBeCloseTo(0.36);
  });

  it('默认特质应放大 arousal 变化（顽皮度 0.5）', () => {
    const delta: AffectiveState = { valence: 0.0, arousal: 0.3, dominance: 0.0 };
    const result = constrainDelta(delta, DEFAULT_TRAITS);
    // 0.3 * (0.5 + 0.5) = 0.3 * 1.0 = 0.3
    expect(result.arousal).toBeCloseTo(0.3);
  });

  it('dominance 不应受特质影响', () => {
    const delta: AffectiveState = { valence: 0.0, arousal: 0.0, dominance: 0.5 };
    const result = constrainDelta(delta, DEFAULT_TRAITS);
    expect(result.dominance).toBe(0.5);
  });

  it('高共情度应大幅放大 valence', () => {
    const delta: AffectiveState = { valence: 0.2, arousal: 0.0, dominance: 0.0 };
    const highEmpathy = { playfulness: 0.5, empathy: 1.0 };
    const result = constrainDelta(delta, highEmpathy);
    // 0.2 * (0.5 + 1.0) = 0.2 * 1.5 = 0.3
    expect(result.valence).toBeCloseTo(0.3);
  });

  it('低共情度应缩小 valence 变化', () => {
    const delta: AffectiveState = { valence: 0.3, arousal: 0.0, dominance: 0.0 };
    const lowEmpathy = { playfulness: 0.5, empathy: 0.0 };
    const result = constrainDelta(delta, lowEmpathy);
    // 0.3 * (0.5 + 0.0) = 0.3 * 0.5 = 0.15
    expect(result.valence).toBeCloseTo(0.15);
  });

  it('负值 delta 应正确缩放', () => {
    const delta: AffectiveState = { valence: -0.2, arousal: 0.0, dominance: 0.0 };
    const result = constrainDelta(delta, DEFAULT_TRAITS);
    // -0.2 * (0.5 + 0.7) = -0.2 * 1.2 = -0.24
    expect(result.valence).toBeCloseTo(-0.24);
  });

  it('零 delta 应返回零', () => {
    const delta: AffectiveState = { valence: 0, arousal: 0, dominance: 0 };
    const result = constrainDelta(delta, DEFAULT_TRAITS);
    expect(result.valence).toBe(0);
    expect(result.arousal).toBe(0);
    expect(result.dominance).toBe(0);
  });
});

/**
 * 大五人格调制测试（架构不变量 A10）
 */
import {
  modulateDelta,
  modulateFull,
  DEFAULT_BIG_FIVE,
} from '../../src/llm-service/personality/traits';

describe('modulateDelta - 架构不变量 A10', () => {
  it('默认大五人格应产生与 constrainDelta 兼容的结果', () => {
    const delta: AffectiveState = { valence: 0.3, arousal: 0.2, dominance: 0.1 };
    // modulateDelta 仅使用默认 BigFive 值
    const result = modulateDelta(delta);
    expect(typeof result.valence).toBe('number');
    expect(typeof result.arousal).toBe('number');
    expect(typeof result.dominance).toBe('number');
  });

  it('高外向性应增强 valence 增益', () => {
    const delta: AffectiveState = { valence: 0.2, arousal: 0, dominance: 0 };
    const lowExtra = modulateDelta(delta, { extraversion: 0.0 });
    const highExtra = modulateDelta(delta, { extraversion: 1.0 });
    // 外向性: valence gain = 0.5 + extraversion
    // low: 0.2 * (0.5+0.0) = 0.1, 再乘神经质 1.3 → 0.13
    // high: 0.2 * (0.5+1.0) = 0.3, 再乘神经质 1.3 → 0.39
    expect(highExtra.valence).toBeGreaterThan(lowExtra.valence);
  });

  it('高神经质应放大所有维度波动', () => {
    const delta: AffectiveState = { valence: 0.2, arousal: 0.2, dominance: 0.2 };
    const lowNeuro = modulateDelta(delta, { neuroticism: 0.0 });
    const highNeuro = modulateDelta(delta, { neuroticism: 1.0 });
    // 神经质: gain = 1 + neuroticism
    // low: 1.0x, high: 2.0x
    expect(highNeuro.valence).toBeGreaterThan(lowNeuro.valence);
    expect(highNeuro.arousal).toBeGreaterThan(lowNeuro.arousal);
    expect(highNeuro.dominance).toBeGreaterThan(lowNeuro.dominance);
  });

  it('高宜人性应衰减负面 delta', () => {
    const delta: AffectiveState = { valence: -0.3, arousal: -0.2, dominance: -0.1 };
    const lowAgree = modulateDelta(delta, { agreeableness: 0.0 });
    const highAgree = modulateDelta(delta, { agreeableness: 1.0 });
    // 宜人性: negative * (1 - agreeableness * 0.5)
    // low: -0.3 * 1.0 = -0.3, high: -0.3 * 0.5 = -0.15
    expect(highAgree.valence).toBeGreaterThan(lowAgree.valence);
    expect(highAgree.arousal).toBeGreaterThan(lowAgree.arousal);
  });

  it('高开放性应增强 arousal 变化', () => {
    const delta: AffectiveState = { valence: 0, arousal: 0.2, dominance: 0 };
    const lowOpen = modulateDelta(delta, { openness: 0.0 });
    const highOpen = modulateDelta(delta, { openness: 1.0 });
    // 开放性: arousal gain = 0.5 + openness
    // low: 0.2 * (0.5+0.0) = 0.1, high: 0.2 * (0.5+1.0) = 0.3
    // 再乘神经质 1.3 → 0.13 vs 0.39
    expect(highOpen.arousal).toBeGreaterThan(lowOpen.arousal);
  });

  it('高尽责性应抑制 dominance 变化', () => {
    const delta: AffectiveState = { valence: 0, arousal: 0, dominance: 0.3 };
    const lowConsc = modulateDelta(delta, { conscientiousness: 0.0 });
    const highConsc = modulateDelta(delta, { conscientiousness: 1.0 });
    // 尽责性: dominance suppress = 1 - conscientiousness * 0.3
    // low: 0.3 * 1.0 = 0.3, high: 0.3 * 0.7 = 0.21
    // 再乘神经质
    const expectedLow = 0.3 * 1.3;
    const expectedHigh = 0.3 * 0.7 * 1.3;
    expect(lowConsc.dominance).toBeCloseTo(expectedLow);
    expect(highConsc.dominance).toBeCloseTo(expectedHigh);
    expect(highConsc.dominance).toBeLessThan(lowConsc.dominance);
  });

  it('空对象不改变行为（架构不变量 A10: 向后兼容）', () => {
    const delta: AffectiveState = { valence: 0.3, arousal: 0.2, dominance: 0.1 };
    const result = modulateDelta(delta, {});
    // 使用默认 BigFive 值，不应出错
    expect(result.valence).not.toBeNaN();
    expect(result.arousal).not.toBeNaN();
    expect(result.dominance).not.toBeNaN();
  });

  it('modulateFull 完整管线应将 constrainDelta 和 modulateDelta 串联', () => {
    const delta: AffectiveState = { valence: 0.3, arousal: 0.2, dominance: 0.1 };
    const traits = { playfulness: 0.5, empathy: 0.7 };
    const bigFive = { extraversion: 0.8, neuroticism: 0.3 };
    const full = modulateFull(delta, traits, bigFive);
    const constrained = constrainDelta(delta, traits);
    const modulated = modulateDelta(constrained, bigFive);
    expect(full.valence).toBeCloseTo(modulated.valence);
    expect(full.arousal).toBeCloseTo(modulated.arousal);
    expect(full.dominance).toBeCloseTo(modulated.dominance);
  });
});
