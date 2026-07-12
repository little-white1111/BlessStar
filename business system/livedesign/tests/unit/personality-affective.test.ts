/**
 * VAD 情感模型单元测试
 * 测试 emotionToVAD 和 vadToEmotion 的映射逻辑
 */
import { describe, it, expect } from 'vitest';
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
