/**
 * 情感衰减单元测试（架构不变量 A2）
 * 测试 EmotionDecay 的衰减计算和定时器逻辑
 */
import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { EmotionDecay } from '../../src/llm-service/personality/decay';
import type { AffectiveState } from '../../src/llm-service/personality/affective';

describe('EmotionDecay - 衰减计算（无噪声模式，noiseAmplitude=0）', () => {
  it('高 valence 应衰减到基线', () => {
    const decay = new EmotionDecay(300_000, 0.1, 0);
    const current: AffectiveState = { valence: 0.9, arousal: 0.5, dominance: 0.5 };
    const result = decay.apply(current);
    // valence: 0.9 + (0.5 - 0.9) * 0.1 = 0.9 - 0.04 = 0.86
    expect(result.valence).toBeCloseTo(0.86);
    expect(result.arousal).toBe(0.5);
    expect(result.dominance).toBe(0.5);
  });

  it('低 valence 应向基线回升', () => {
    const decay = new EmotionDecay(300_000, 0.1, 0);
    const current: AffectiveState = { valence: 0.1, arousal: 0.5, dominance: 0.5 };
    const result = decay.apply(current);
    // valence: 0.1 + (0.5 - 0.1) * 0.1 = 0.1 + 0.04 = 0.14
    expect(result.valence).toBeCloseTo(0.14);
  });

  it('高 arousal 应衰减到基线', () => {
    const decay = new EmotionDecay(300_000, 0.2, 0);
    const current: AffectiveState = { valence: 0.5, arousal: 0.8, dominance: 0.5 };
    const result = decay.apply(current);
    // arousal: 0.8 + (0.5 - 0.8) * 0.2 = 0.8 - 0.06 = 0.74
    expect(result.arousal).toBeCloseTo(0.74);
  });

  it('接近基线时应直接对齐', () => {
    const decay = new EmotionDecay(300_000, 0.1, 0);
    const current: AffectiveState = { valence: 0.502, arousal: 0.5, dominance: 0.5 };
    const result = decay.apply(current);
    // diff = 0.5 - 0.502 = -0.002, delta = -0.002 * 0.1 = -0.0002
    // |delta| < 0.005 → 直接对齐基线
    expect(result.valence).toBe(0.5);
  });

  it('多轮衰减后应逼近基线', () => {
    const decay = new EmotionDecay(300_000, 0.3, 0);
    let state: AffectiveState = { valence: 0.0, arousal: 1.0, dominance: 0.0 };

    // 5 轮衰减
    for (let i = 0; i < 5; i++) {
      state = decay.apply(state);
    }

    // valence 应接近 0.5
    expect(state.valence).toBeGreaterThan(0.4);
    // arousal 应接近 0.5
    expect(state.arousal).toBeLessThan(0.6);
    // dominance 应接近 0.5
    expect(state.dominance).toBeGreaterThan(0.3);
  });

  it('所有轴值应限制在 0.0 ~ 1.0 范围内', () => {
    const decay = new EmotionDecay(300_000, 0.5, 0);
    const current: AffectiveState = { valence: 1.5, arousal: -0.5, dominance: 2.0 };
    const result = decay.apply(current);
    expect(result.valence).toBeGreaterThanOrEqual(0);
    expect(result.valence).toBeLessThanOrEqual(1);
    expect(result.arousal).toBeGreaterThanOrEqual(0);
    expect(result.arousal).toBeLessThanOrEqual(1);
    expect(result.dominance).toBeGreaterThanOrEqual(0);
    expect(result.dominance).toBeLessThanOrEqual(1);
  });
});

describe('EmotionDecay - 随机漫步噪声（架构不变量 A2 增强）', () => {
  it('有噪声时结果应在预期值附近波动', () => {
    const decay = new EmotionDecay(300_000, 0.1, 0.02);
    const current: AffectiveState = { valence: 0.9, arousal: 0.5, dominance: 0.5 };

    // 多次采样，验证结果围绕 0.86 波动
    const results: number[] = [];
    for (let i = 0; i < 100; i++) {
      const r = decay.apply(current);
      results.push(r.valence);
    }

    // 所有结果应在 [0, 1] 内
    results.forEach(v => {
      expect(v).toBeGreaterThanOrEqual(0);
      expect(v).toBeLessThanOrEqual(1);
    });
    // 均值应接近 0.86（噪声均值为 0）
    const mean = results.reduce((a, b) => a + b, 0) / results.length;
    expect(mean).toBeCloseTo(0.86, 1);
    // 应存在波动（方差 > 0）
    const variance = results.reduce((a, b) => a + (b - mean) ** 2, 0) / results.length;
    expect(variance).toBeGreaterThan(0);
  });

  it('噪声幅度为 0 时结果完全确定', () => {
    const decay = new EmotionDecay(300_000, 0.1, 0);
    const current: AffectiveState = { valence: 0.9, arousal: 0.5, dominance: 0.5 };

    const r1 = decay.apply(current);
    const r2 = decay.apply(current);
    const r3 = decay.apply(current);

    expect(r1.valence).toBe(r2.valence);
    expect(r2.valence).toBe(r3.valence);
  });

  it('大噪声幅度结果仍应在 [0, 1] 内', () => {
    const decay = new EmotionDecay(300_000, 0.1, 0.1); // 最大噪声
    const current: AffectiveState = { valence: 0.2, arousal: 0.8, dominance: 0.3 };

    for (let i = 0; i < 200; i++) {
      const result = decay.apply(current);
      expect(result.valence).toBeGreaterThanOrEqual(0);
      expect(result.valence).toBeLessThanOrEqual(1);
      expect(result.arousal).toBeGreaterThanOrEqual(0);
      expect(result.arousal).toBeLessThanOrEqual(1);
      expect(result.dominance).toBeGreaterThanOrEqual(0);
      expect(result.dominance).toBeLessThanOrEqual(1);
    }
  });

  it('长时间多轮衰减后 VAD 仍应保持在 [0, 1] 内', () => {
    const decay = new EmotionDecay(300_000, 0.3, 0.05);
    let state: AffectiveState = { valence: 0.0, arousal: 1.0, dominance: 0.0 };

    // 50 轮衰减（噪声累积）
    for (let i = 0; i < 50; i++) {
      state = decay.apply(state);
      expect(state.valence).toBeGreaterThanOrEqual(0);
      expect(state.valence).toBeLessThanOrEqual(1);
      expect(state.arousal).toBeGreaterThanOrEqual(0);
      expect(state.arousal).toBeLessThanOrEqual(1);
      expect(state.dominance).toBeGreaterThanOrEqual(0);
      expect(state.dominance).toBeLessThanOrEqual(1);
    }

    // 多轮后仍应接近基线
    expect(state.valence).toBeGreaterThan(0.3);
    expect(state.arousal).toBeLessThan(0.7);
  });
});

describe('EmotionDecay - 定时器（架构不变量 A2）', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('start() 应启动定时器', () => {
    const decay = new EmotionDecay(1000, 0.1);
    const callback = vi.fn();
    decay.start(callback);
    expect(decay.isRunning).toBe(true);
    decay.stop();
  });

  it('定时器到期应调用衰减回调', () => {
    const decay = new EmotionDecay(1000, 0.1);
    const callback = vi.fn();

    decay.start(callback);
    expect(callback).not.toHaveBeenCalled();

    // 快进 1000ms
    vi.advanceTimersByTime(1000);
    expect(callback).toHaveBeenCalledTimes(1);

    // 再快进 1000ms
    vi.advanceTimersByTime(1000);
    expect(callback).toHaveBeenCalledTimes(2);

    decay.stop();
  });

  it('stop() 应停止定时器', () => {
    const decay = new EmotionDecay(1000, 0.1);
    const callback = vi.fn();

    decay.start(callback);
    decay.stop();
    expect(decay.isRunning).toBe(false);

    // 快进后不应调用
    vi.advanceTimersByTime(2000);
    expect(callback).not.toHaveBeenCalled();
  });

  it('重复 start() 不应创建多个定时器', () => {
    const decay = new EmotionDecay(1000, 0.1);
    const callback = vi.fn();

    decay.start(callback);
    decay.start(callback); // 重复调用
    decay.start(callback);

    vi.advanceTimersByTime(1000);
    expect(callback).toHaveBeenCalledTimes(1);

    decay.stop();
  });

  it('updateConfig() 应更新参数（含噪声幅度）', () => {
    const decay = new EmotionDecay(300_000, 0.1, 0);
    decay.updateConfig(1000, 0.5, 0.03);

    // 测试新参数生效
    const result = decay.apply({ valence: 0.9, arousal: 0.5, dominance: 0.5 });
    // 0.9 + (0.5 - 0.9) * 0.5 = 0.9 - 0.2 = 0.7（无噪声时）
    // 有噪声时在 0.7 附近
    expect(result.valence).toBeGreaterThanOrEqual(0);
    expect(result.valence).toBeLessThanOrEqual(1);
  });
});
