/**
 * VAD 情感模型单元测试
 * 测试 emotionToVAD 和 vadToEmotion 的映射逻辑
 */
import { describe, it, expect, beforeEach } from 'vitest';
import {
  emotionToVAD,
  vadToEmotion,
  BASELINE_STATE,
  EMOTION_VAD_MAP,
} from '../../src/llm-service/personality/affective';

describe('emotionToVAD', () => {
  it('应返回已知情绪的 VAD 值', () => {
    const vad = emotionToVAD('happy');
    expect(vad.valence).toBeGreaterThan(0.5);
    expect(vad.arousal).toBeGreaterThan(0.5);
  });

  it('未知情绪应返回基线状态', () => {
    const vad = emotionToVAD('nonexistent_emotion');
    expect(vad.valence).toBe(0.5);
    expect(vad.arousal).toBe(0.5);
    expect(vad.dominance).toBe(0.5);
  });

  it('应不区分大小写', () => {
    const upper = emotionToVAD('HAPPY');
    const lower = emotionToVAD('happy');
    expect(upper.valence).toBe(lower.valence);
  });

  it('所有非 neutral 的情绪都应返回非基线值', () => {
    for (const emotion of Object.keys(EMOTION_VAD_MAP)) {
      if (emotion === 'neutral') continue; // neutral 定义为基线状态
      const vad = emotionToVAD(emotion);
      const isDifferent =
        vad.valence !== BASELINE_STATE.valence ||
        vad.arousal !== BASELINE_STATE.arousal ||
        vad.dominance !== BASELINE_STATE.dominance;
      expect(isDifferent, `${emotion} 应不同于基线`).toBe(true);
    }
  });

  it('neutral 情绪应等于基线状态', () => {
    const vad = emotionToVAD('neutral');
    expect(vad.valence).toBe(BASELINE_STATE.valence);
    expect(vad.arousal).toBe(BASELINE_STATE.arousal);
    expect(vad.dominance).toBe(BASELINE_STATE.dominance);
  });

  it('sad 的 valence 应低于中性', () => {
    const vad = emotionToVAD('sad');
    expect(vad.valence).toBeLessThan(0.5);
  });

  it('angry 的 arousal 应较高', () => {
    const vad = emotionToVAD('angry');
    expect(vad.arousal).toBeGreaterThan(0.7);
  });
});

describe('vadToEmotion', () => {
  it('应返回中性状态的 closest emotion', () => {
    const emotion = vadToEmotion({ valence: 0.5, arousal: 0.5, dominance: 0.5 });
    expect(typeof emotion).toBe('string');
  });

  it('高 valence 低 arousal 应映射到 relaxed', () => {
    const emotion = vadToEmotion({ valence: 0.7, arousal: 0.15, dominance: 0.5 });
    expect(emotion).toBe('relaxed');
  });

  it('低 valence 高 arousal 应映射到 angry', () => {
    const emotion = vadToEmotion({ valence: 0.1, arousal: 0.85, dominance: 0.8 });
    expect(emotion).toBe('angry');
  });

  it('低 valence 低 arousal 应映射到 sad', () => {
    const emotion = vadToEmotion({ valence: 0.15, arousal: 0.3, dominance: 0.3 });
    expect(emotion).toBe('sad');
  });
});

/**
 * VAD 概率场 — ProbabilityDensityMatcher 测试
 * 架构不变量 A9: 情感标签映射必须输出 Top-3 概率
 */
import { ProbabilityDensityMatcher } from '../../src/llm-service/matching/probability-matcher';
import { vadToEmotionProbabilities, resetDefaultMatcher } from '../../src/llm-service/personality/affective';

describe('ProbabilityDensityMatcher - 架构不变量 A9', () => {
  it('中性状态应返回 top-3 概率且 neutral 概率最高', () => {
    const matcher = new ProbabilityDensityMatcher();
    const results = matcher.match({ valence: 0.5, arousal: 0.5, dominance: 0.5 });
    expect(results.length).toBeGreaterThanOrEqual(1);
    expect(results.length).toBeLessThanOrEqual(3);
    expect(results[0].emotion).toBe('neutral');
    expect(results[0].probability).toBeGreaterThan(0);
  });

  it('高 valence 高 arousal 时 happy 或 joy 应在 Top-3', () => {
    const matcher = new ProbabilityDensityMatcher();
    const results = matcher.match({ valence: 0.9, arousal: 0.8, dominance: 0.6 });
    const emotions = results.map((r) => r.emotion);
    const hasPositive = emotions.some((e) => ['happy', 'joy', 'excited', 'love'].includes(e));
    expect(hasPositive).toBe(true);
  });

  it('低 valence 低 arousal 时 sad 应在 Top-3', () => {
    const matcher = new ProbabilityDensityMatcher();
    const results = matcher.match({ valence: 0.15, arousal: 0.3, dominance: 0.3 });
    const emotions = results.map((r) => r.emotion);
    expect(emotions).toContain('sad');
  });

  it('低 valence 高 arousal 时 angry 应在 Top-3', () => {
    const matcher = new ProbabilityDensityMatcher();
    const results = matcher.match({ valence: 0.1, arousal: 0.85, dominance: 0.8 });
    const emotions = results.map((r) => r.emotion);
    expect(emotions).toContain('angry');
  });

  it('Top-3 概率和应有一个合理的分布', () => {
    const matcher = new ProbabilityDensityMatcher();
    const results = matcher.match({ valence: 0.7, arousal: 0.3, dominance: 0.5 });
    const totalProb = results.reduce((sum, r) => sum + r.probability, 0);
    // 22 分类 softmax 下 Top-3 总和通常 > 0.3
    expect(totalProb).toBeGreaterThan(0.3);
    expect(totalProb).toBeLessThanOrEqual(1.0);
  });

  it('stdScale 增大应产生更均衡的概率分布', () => {
    const matcher1 = new ProbabilityDensityMatcher({ stdScale: 1.0 });
    const matcher3 = new ProbabilityDensityMatcher({ stdScale: 3.0 });
    const results1 = matcher1.match({ valence: 0.8, arousal: 0.7, dominance: 0.6 });
    const results3 = matcher3.match({ valence: 0.8, arousal: 0.7, dominance: 0.6 });
    // 大 stdScale 下 top1 概率应更低（分布更均衡）
    expect(results1[0].probability).toBeGreaterThan(results3[0].probability);
  });

  it('vacuum 区（冷静恶意）应返回有意义的 Top-3 分布', () => {
    // 测试 VAD 空间真空区：冷静恶意 (0.15, 0.20, 0.85)
    const matcher = new ProbabilityDensityMatcher();
    const results = matcher.match({ valence: 0.15, arousal: 0.20, dominance: 0.85 });
    expect(results.length).toBeGreaterThanOrEqual(1);
    expect(results.length).toBeLessThanOrEqual(3);
    // 至少有匹配结果
    expect(results[0].probability).toBeGreaterThan(0);
  });
});

describe('vadToEmotionProbabilities - 便捷函数', () => {
  beforeEach(() => {
    resetDefaultMatcher();
  });

  it('应返回 Top-3 概率结果', () => {
    const results = vadToEmotionProbabilities({ valence: 0.9, arousal: 0.7, dominance: 0.6 });
    expect(results.length).toBeGreaterThanOrEqual(1);
    expect(results.length).toBeLessThanOrEqual(3);
  });

  it('概率值应在 0~1 范围内', () => {
    const results = vadToEmotionProbabilities({ valence: 0.5, arousal: 0.5, dominance: 0.5 });
    for (const r of results) {
      expect(r.probability).toBeGreaterThanOrEqual(0);
      expect(r.probability).toBeLessThanOrEqual(1);
    }
  });

  it('多次调用应返回一致的结果（确定性）', () => {
    const r1 = vadToEmotionProbabilities({ valence: 0.3, arousal: 0.6, dominance: 0.4 });
    resetDefaultMatcher();
    const r2 = vadToEmotionProbabilities({ valence: 0.3, arousal: 0.6, dominance: 0.4 });
    expect(r1.length).toBe(r2.length);
    expect(r1[0].emotion).toBe(r2[0].emotion);
  });
});
