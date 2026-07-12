/**
 * EmotionTriggerMatcher（情绪触发匹配器）
 *
 * 架构不变量 D4: EmotionTrigger 基于 VAD 偏离基线的幅度匹配，
 * 使用 |currentVAD - baselineVAD| > threshold，不使用绝对 VAD 值。
 *
 * 从 config-schema.yaml:
 *   catchphrase.emotion.trigger_threshold - 默认 0.3
 *   catchphrase.emotion.valence_weight - 默认 0.6
 */

import { BASELINE_STATE } from '../personality/affective';
import type { AffectiveState } from '../personality/affective';
import type { EmotionMatchContext } from './types';

/** 默认触发阈值 */
const DEFAULT_THRESHOLD = 0.3;

/** 默认 Valence 权重 */
const DEFAULT_VALENCE_WEIGHT = 0.6;

/** Arousal 权重（由 valence_weight 推导） */
const AROUSAL_WEIGHT = 0.25;
const DOMINANCE_WEIGHT = 0.15;

/** 情绪类型 → 偏差方向匹配规则 */
const EMOTION_DIRECTION_MAP: Record<string, { axis: 'valence' | 'arousal' | 'dominance'; direction: 'positive' | 'negative' }> = {
  encouragement: { axis: 'valence', direction: 'negative' },
  calming: { axis: 'arousal', direction: 'positive' },
};

export class EmotionTriggerMatcher {
  private threshold: number;
  private valenceWeight: number;

  constructor(threshold = DEFAULT_THRESHOLD, valenceWeight = DEFAULT_VALENCE_WEIGHT) {
    this.threshold = threshold;
    this.valenceWeight = valenceWeight;
  }

  /**
   * 更新配置
   */
  updateConfig(threshold: number, valenceWeight: number): void {
    this.threshold = threshold;
    this.valenceWeight = valenceWeight;
  }

  /**
   * 计算 VAD 偏差上下文
   * 架构不变量 D4: 基于 |currentVAD - baselineVAD| 计算偏差。
   *
   * @param currentVAD 当前 VAD 状态
   * @param baselineVAD VAD 基线（默认使用 BASELINE_STATE）
   */
  computeContext(currentVAD: AffectiveState, baselineVAD: AffectiveState = BASELINE_STATE): EmotionMatchContext {
    const dValence = Math.abs(currentVAD.valence - baselineVAD.valence);
    const dArousal = Math.abs(currentVAD.arousal - baselineVAD.arousal);
    const dDominance = Math.abs(currentVAD.dominance - baselineVAD.dominance);

    // 加权偏差幅度（架构不变量 D4）
    const deviation =
      dValence * this.valenceWeight +
      dArousal * AROUSAL_WEIGHT +
      dDominance * DOMINANCE_WEIGHT;

    // 主导轴
    const maxDelta = Math.max(dValence, dArousal, dDominance);
    let dominantAxis: 'valence' | 'arousal' | 'dominance';
    let deviationDirection: 'positive' | 'negative';

    if (maxDelta === dValence) {
      dominantAxis = 'valence';
      deviationDirection = currentVAD.valence > baselineVAD.valence ? 'positive' : 'negative';
    } else if (maxDelta === dArousal) {
      dominantAxis = 'arousal';
      deviationDirection = currentVAD.arousal > baselineVAD.arousal ? 'positive' : 'negative';
    } else {
      dominantAxis = 'dominance';
      deviationDirection = currentVAD.dominance > baselineVAD.dominance ? 'positive' : 'negative';
    }

    return { currentVAD, baselineVAD, deviation, dominantAxis, deviationDirection };
  }

  /**
   * 检测是否触发情绪匹配
   * 架构不变量 D4: |currentVAD - baselineVAD| > threshold
   */
  isTriggered(context: EmotionMatchContext): boolean {
    return context.deviation > this.threshold;
  }

  /**
   * 获取匹配的 Core 口头禅情绪类型
   * 根据偏差方向和主导轴匹配最合适的情绪类型。
   */
  matchEmotionType(context: EmotionMatchContext): string | undefined {
    if (!this.isTriggered(context)) return undefined;

    // 优先匹配完全符合的
    for (const [emotionType, rule] of Object.entries(EMOTION_DIRECTION_MAP)) {
      if (rule.axis === context.dominantAxis && rule.direction === context.deviationDirection) {
        return emotionType;
      }
    }

    // 次优匹配：仅匹配轴
    for (const [emotionType, rule] of Object.entries(EMOTION_DIRECTION_MAP)) {
      if (rule.axis === context.dominantAxis) {
        return emotionType;
      }
    }

    // 默认
    return context.deviationDirection === 'negative' ? 'encouragement' : 'calming';
  }

  /** 获取当前阈值 */
  getThreshold(): number {
    return this.threshold;
  }

  /** 获取当前 valence 权重 */
  getValenceWeight(): number {
    return this.valenceWeight;
  }
}
