/**
 * 稳定人格特质定义
 *
 * 架构不变量 A1: 人格特质约束 VAD 情感数值的变更方向和幅度。
 * 特质值范围均为 0.0 ~ 1.0。
 */

import type { AffectiveState } from './affective';

/** 人格特质集合 */
export interface PersonalityTraits {
  /** 顽皮度: 0.0=严肃, 1.0=爱开玩笑。控制情感向活跃方向的偏移幅度 */
  playfulness: number;
  /** 共情度: 0.0=冷漠, 1.0=高度共情。控制情感向用户情绪靠拢的强度 */
  empathy: number;
}

/** 默认特质值 */
export const DEFAULT_TRAITS: PersonalityTraits = {
  playfulness: 0.5,
  empathy: 0.7,
};

/**
 * 使用特质约束 VAD 变化量。
 * traits.playfulness: 放大 arousal 变化（顽皮 → 更容易兴奋）
 * traits.empathy: 放大 valence 变化（共情 → 更容易被用户情绪感染）
 *
 * @param delta 原始的 VAD 变化量
 * @param traits 当前人格特质
 * @returns 经特质约束后的 VAD 变化量
 */
export function constrainDelta(
  delta: AffectiveState,
  traits: PersonalityTraits
): AffectiveState {
  return {
    // 共情度放大 valence 变化
    valence: delta.valence * (0.5 + traits.empathy),
    // 顽皮度放大 arousal 变化
    arousal: delta.arousal * (0.5 + traits.playfulness),
    // 支配度不受特质影响
    dominance: delta.dominance,
  };
}
