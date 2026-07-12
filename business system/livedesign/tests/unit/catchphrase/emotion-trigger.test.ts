/**
 * EmotionTriggerMatcher 单元测试
 *
 * 架构不变量 D4: EmotionTrigger 基于 VAD 偏离基线的幅度匹配。
 */

import { describe, it, expect } from 'vitest';
import { EmotionTriggerMatcher } from '../../../src/llm-service/catchphrase/emotion-trigger';
import { BASELINE_STATE } from '../../../src/llm-service/personality/affective';

describe('EmotionTriggerMatcher - computeContext（架构不变量 D4）', () => {
  const matcher = new EmotionTriggerMatcher(0.3, 0.6);

  it('基线状态偏差应为 0', () => {
    const ctx = matcher.computeContext({ valence: 0.5, arousal: 0.5, dominance: 0.5 });
    expect(ctx.deviation).toBe(0);
    expect(ctx.dominantAxis).toBe('valence');
  });

  it('valence 偏离正方向应被正确检测', () => {
    const ctx = matcher.computeContext({ valence: 0.9, arousal: 0.5, dominance: 0.5 });
    expect(ctx.deviation).toBeGreaterThan(0);
    expect(ctx.dominantAxis).toBe('valence');
    expect(ctx.deviationDirection).toBe('positive');
  });

  it('valence 偏离负方向应被正确检测', () => {
    const ctx = matcher.computeContext({ valence: 0.1, arousal: 0.5, dominance: 0.5 });
    expect(ctx.deviation).toBeGreaterThan(0);
    expect(ctx.dominantAxis).toBe('valence');
    expect(ctx.deviationDirection).toBe('negative');
  });

  it('arousal 为主轴时应正确识别', () => {
    const ctx = matcher.computeContext({ valence: 0.5, arousal: 0.9, dominance: 0.5 });
    expect(ctx.dominantAxis).toBe('arousal');
    expect(ctx.deviationDirection).toBe('positive');
  });

  it('dominance 为主轴时应正确识别', () => {
    const ctx = matcher.computeContext({ valence: 0.5, arousal: 0.5, dominance: 0.9 });
    expect(ctx.dominantAxis).toBe('dominance');
  });
});

describe('EmotionTriggerMatcher - isTriggered', () => {
  it('偏差超过阈值应触发', () => {
    const matcher = new EmotionTriggerMatcher(0.2, 0.6);
    // valence: |0.9-0.5|*0.6 = 0.24 > 0.2
    const ctx = matcher.computeContext({ valence: 0.9, arousal: 0.5, dominance: 0.5 });
    expect(matcher.isTriggered(ctx)).toBe(true);
  });

  it('偏差低于阈值不应触发', () => {
    const matcher = new EmotionTriggerMatcher(0.8, 0.6);
    const ctx = matcher.computeContext({ valence: 0.6, arousal: 0.5, dominance: 0.5 });
    expect(matcher.isTriggered(ctx)).toBe(false);
  });

  it('偏差等于阈值不应触发（严格大于）', () => {
    const matcher = new EmotionTriggerMatcher(0.3, 0.6);
    // valence: |0.0-0.5|*0.6 = 0.3 = threshold, not >
    const ctx = matcher.computeContext({ valence: 0.0, arousal: 0.5, dominance: 0.5 });
    expect(matcher.isTriggered(ctx)).toBe(false);
  });
});

describe('EmotionTriggerMatcher - matchEmotionType', () => {
  it('valence 负方向偏离应匹配 encouragement', () => {
    const matcher = new EmotionTriggerMatcher(0.2, 0.6);
    // deviation = |0.1-0.5|*0.6 = 0.24 > 0.2
    const ctx = matcher.computeContext({ valence: 0.1, arousal: 0.5, dominance: 0.5 });
    const matched = matcher.matchEmotionType(ctx);
    expect(matched).toBe('encouragement');
  });

  it('arousal 正方向偏离应匹配 calming', () => {
    const matcher = new EmotionTriggerMatcher(0.2, 0.6);
    // deviation = |0.8-0.5|*0.6 + |0.9-0.5|*0.25 = 0.18+0.1=0.28 > 0.2 ✓
    const ctx = matcher.computeContext({ valence: 0.8, arousal: 0.9, dominance: 0.5 });
    const matched = matcher.matchEmotionType(ctx);
    expect(matched).toBe('calming');
  });

  it('未触发时 matchEmotionType 应返回 undefined', () => {
    const matcher = new EmotionTriggerMatcher(0.8, 0.6);
    const ctx = matcher.computeContext({ valence: 0.5, arousal: 0.5, dominance: 0.5 });
    expect(matcher.matchEmotionType(ctx)).toBeUndefined();
  });

  it('updateConfig 应正确更新参数', () => {
    const matcher = new EmotionTriggerMatcher(0.3, 0.6);
    matcher.updateConfig(0.5, 0.8);
    expect(matcher.getThreshold()).toBe(0.5);
    expect(matcher.getValenceWeight()).toBe(0.8);
  });
});
