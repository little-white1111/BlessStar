/**
 * 情感衰减算法（架构不变量 A2）
 *
 * 情感衰减使 VAD 值随自然时间回归基线（0.5, 0.5, 0.5），
 * 模拟人类情感的"平复"过程。
 *
 * 衰减公式: newVAD = currentVAD + (baseline - currentVAD) * rate
 * 即指数衰减逼近基线，rate 越大衰减越快。
 */

import type { AffectiveState } from './affective';
import { BASELINE_STATE } from './affective';

/** 情感衰减器 */
export class EmotionDecay {
  private timer: ReturnType<typeof setInterval> | null = null;

  /**
   * @param intervalMs 衰减运行间隔（毫秒），默认 300000 (5 分钟)
   * @param rate 单次衰减速率 (0.01~0.5)，默认 0.1
   * @param noiseAmplitude 随机漫步噪声幅度 (0.0~0.1)，默认 0.02
   */
  constructor(
    private intervalMs: number = 300_000,
    private rate: number = 0.1,
    private noiseAmplitude: number = 0.02
  ) {}

  /**
   * 执行单次衰减计算。
   * 将 current 向 baseline 逼近 rate 比例。
   */
  apply(current: AffectiveState): AffectiveState {
    return {
      valence: this.decayAxis(current.valence, BASELINE_STATE.valence),
      arousal: this.decayAxis(current.arousal, BASELINE_STATE.arousal),
      dominance: this.decayAxis(current.dominance, BASELINE_STATE.dominance),
    };
  }

  /**
   * 单轴衰减: target += (baseline - target) * rate + 随机漫步噪声
   *
   * 架构不变量 A2（增强）：衰减中加入可配置随机漫步噪声，
   * 模拟自然情感波动，但仍保证回归基线。
   */
  private decayAxis(current: number, baseline: number): number {
    const diff = baseline - current;
    const regression = diff * this.rate;
    const noise = (Math.random() - 0.5) * 2 * this.noiseAmplitude;
    const delta = regression + noise;
    // 当变化量小于 0.005 时直接对齐基线，防止永不休止的微小振荡
    if (Math.abs(delta) < 0.005) {
      return baseline;
    }
    return this.clamp(current + delta);
  }

  /** 将值限制在 0.0 ~ 1.0 范围内 */
  private clamp(value: number): number {
    return Math.max(0, Math.min(1, value));
  }

  /**
   * 启动定时衰减。
   * 架构不变量 A2: 情感衰减不可跳过 — 除非调用 destroy()，否则定时器一直运行。
   *
   * @param onDecay 每次衰减时的回调，接收衰减后的 VAD 状态
   */
  start(onDecay: (state: AffectiveState) => void): void {
    if (this.timer) {
      return; // 已经启动
    }
    this.timer = setInterval(() => {
      // 回调由 PersonalityEngine 注入，PersonalityEngine 维护实际 VAD 状态
      onDecay({} as AffectiveState);
    }, this.intervalMs);
  }

  /** 停止衰减定时器 */
  stop(): void {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }

  /** 更新衰减参数（含噪声幅度） */
  updateConfig(intervalMs: number, rate: number, noiseAmplitude?: number): void {
    this.intervalMs = intervalMs;
    this.rate = rate;
    if (noiseAmplitude !== undefined) {
      this.noiseAmplitude = noiseAmplitude;
    }
  }

  /** 是否正在运行 */
  get isRunning(): boolean {
    return this.timer !== null;
  }
}
