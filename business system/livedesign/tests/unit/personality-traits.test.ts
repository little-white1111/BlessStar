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
