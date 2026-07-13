/**
 * VAD 情感模型 (Valence/Arousal/Dominance)
 *
 * 三轴情感模型，用于描述和量化情感状态。
 * 架构不变量 A1: 所有情感数值的变更必须经过 PersonalityEngine 的 trait 约束。
 *
 * Valence (效价):   0.0 (负面) ~ 0.5 (中性) ~ 1.0 (正面)
 * Arousal (唤醒度):  0.0 (平静) ~ 0.5 (中性) ~ 1.0 (兴奋)
 * Dominance (支配度): 0.0 (顺从) ~ 0.5 (中性) ~ 1.0 (支配)
 */

import { ProbabilityDensityMatcher } from '../matching/probability-matcher';
import type { MatchResult } from '../matching/types';

/** VAD 三轴情感状态 */
export interface AffectiveState {
  /** 效价: 0.0=负面, 0.5=中性, 1.0=正面 */
  valence: number;
  /** 唤醒度: 0.0=平静, 0.5=中性, 1.0=兴奋 */
  arousal: number;
  /** 支配度: 0.0=顺从, 0.5=中性, 1.0=支配 */
  dominance: number;
}

/** VAD 情感的默认基线（全中性） */
export const BASELINE_STATE: AffectiveState = {
  valence: 0.5,
  arousal: 0.5,
  dominance: 0.5,
};

/** 常见情感的 VAD 映射（基于 Russell 的 Circumplex Model 扩展） */
export const EMOTION_VAD_MAP: Record<string, Partial<AffectiveState>> = {
  happy: { valence: 0.85, arousal: 0.7, dominance: 0.6 },
  joy: { valence: 0.9, arousal: 0.75, dominance: 0.55 },
  excited: { valence: 0.8, arousal: 0.9, dominance: 0.7 },
  surprised: { valence: 0.5, arousal: 0.85, dominance: 0.5 },
  calm: { valence: 0.6, arousal: 0.2, dominance: 0.5 },
  relaxed: { valence: 0.7, arousal: 0.15, dominance: 0.5 },
  neutral: { valence: 0.5, arousal: 0.5, dominance: 0.5 },
  sad: { valence: 0.15, arousal: 0.3, dominance: 0.3 },
  sorrow: { valence: 0.1, arousal: 0.2, dominance: 0.25 },
  angry: { valence: 0.1, arousal: 0.85, dominance: 0.8 },
  rage: { valence: 0.05, arousal: 0.95, dominance: 0.9 },
  anxious: { valence: 0.2, arousal: 0.8, dominance: 0.2 },
  fear: { valence: 0.1, arousal: 0.9, dominance: 0.15 },
  confused: { valence: 0.3, arousal: 0.6, dominance: 0.3 },
  shy: { valence: 0.4, arousal: 0.4, dominance: 0.2 },
  embarrassed: { valence: 0.2, arousal: 0.6, dominance: 0.2 },
  tired: { valence: 0.3, arousal: 0.15, dominance: 0.35 },
  sympathy: { valence: 0.7, arousal: 0.4, dominance: 0.3 },
  love: { valence: 0.9, arousal: 0.6, dominance: 0.5 },
  grateful: { valence: 0.85, arousal: 0.5, dominance: 0.4 },
  proud: { valence: 0.8, arousal: 0.6, dominance: 0.8 },
};

/** 将情绪名称映射到 VAD 状态 */
export function emotionToVAD(emotion: string): AffectiveState {
  const vad = EMOTION_VAD_MAP[emotion.toLowerCase()];
  if (vad) {
    return { ...BASELINE_STATE, ...vad };
  }
  return { ...BASELINE_STATE };
}

/** 将 VAD 状态映射到最接近的情绪名称（向后兼容的最近邻匹配） */
export function vadToEmotion(state: AffectiveState): string {
  let closest = 'neutral';
  let minDistance = Infinity;

  for (const [emotion, vad] of Object.entries(EMOTION_VAD_MAP)) {
    const d = Math.sqrt(
      (state.valence - (vad.valence ?? 0.5)) ** 2 +
      (state.arousal - (vad.arousal ?? 0.5)) ** 2 +
      (state.dominance - (vad.dominance ?? 0.5)) ** 2
    );
    if (d < minDistance) {
      minDistance = d;
      closest = emotion;
    }
  }

  return closest;
}

/**
 * 使用高斯 PDF 将 VAD 状态映射到 Top-3 概率情绪标签。
 * 架构不变量 A9: 情感标签映射必须输出 Top-3 概率。
 *
 * 这是一种便捷函数，内部创建临时 ProbabilityDensityMatcher 实例。
 * 如需高性能重复匹配，请直接使用 ProbabilityDensityMatcher 类。
 */
let _defaultMatcher: ProbabilityDensityMatcher | null = null;

function getDefaultMatcher(): ProbabilityDensityMatcher {
  if (!_defaultMatcher) {
    _defaultMatcher = new ProbabilityDensityMatcher();
  }
  return _defaultMatcher;
}

/** 重置默认匹配器（用于测试或配置更新） */
export function resetDefaultMatcher(): void {
  _defaultMatcher = null;
}

/**
 * 将 VAD 状态映射到 Top-3 概率情绪标签。
 * 架构不变量 A9: 返回数组长度 ≥ 1 且 ≤ 3，概率和 ≈ 1.0。
 *
 * @param state 当前 VAD 状态
 * @returns MatchResult[] — Top-3 概率结果
 */
export function vadToEmotionProbabilities(state: AffectiveState): MatchResult[] {
  return getDefaultMatcher().match(state);
}
