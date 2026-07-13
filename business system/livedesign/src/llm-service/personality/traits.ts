/**
 * 稳定人格特质定义
 *
 * 架构不变量 A1: 人格特质约束 VAD 情感数值的变更方向和幅度。
 * 架构不变量 A10: 人格特质扩展不得破坏现有接口 — BigFiveTraits 使用 Partial<>。
 *
 * 特质值范围均为 0.0 ~ 1.0。
 *
 * 大五人格调制矩阵（架构不变量 A10 新增）:
 *   - extraversion → V baseline offset（外向性提升 V 基线）
 *   - neuroticism → gain amplification（神经质放大情感波动幅度）
 *   - agreeableness → negative delta decay（宜人性衰减负面 delta）
 *   - openness → A baseline offset（开放性提升 A 基线）
 *   - conscientiousness → D stability（尽责性抑制 D 变化）
 */

import type { AffectiveState } from './affective';

/** 人格特质集合（向后兼容） */
export interface PersonalityTraits {
  /** 顽皮度: 0.0=严肃, 1.0=爱开玩笑。控制情感向活跃方向的偏移幅度 */
  playfulness: number;
  /** 共情度: 0.0=冷漠, 1.0=高度共情。控制情感向用户情绪靠拢的强度 */
  empathy: number;
}

/**
 * 大五人格特质扩展。
 * 架构不变量 A10: 使用 Partial<> 确保向后兼容。
 */
export type BigFiveTraits = Partial<{
  /** 外向性: 0.0=内向, 1.0=外向。影响 V 基线偏移（外向→V 更高） */
  extraversion: number;
  /** 神经质: 0.0=情绪稳定, 1.0=神经质。放大情感波动幅度 */
  neuroticism: number;
  /** 宜人性: 0.0=对抗, 1.0=宜人。衰减负面 delta */
  agreeableness: number;
  /** 开放性: 0.0=封闭, 1.0=开放。影响 A 基线偏移 */
  openness: number;
  /** 尽责性: 0.0=随性, 1.0=尽责。抑制 D 变化 */
  conscientiousness: number;
}>;

/** 默认特质值 */
export const DEFAULT_TRAITS: PersonalityTraits = {
  playfulness: 0.5,
  empathy: 0.7,
};

/** 大五人格默认值（架构不变量 A10: 新特质默认为 0.5 中性，不影响现有行为） */
export const DEFAULT_BIG_FIVE: Required<BigFiveTraits> = {
  extraversion: 0.5,
  neuroticism: 0.3,
  agreeableness: 0.7,
  openness: 0.5,
  conscientiousness: 0.6,
};

/**
 * 使用基础特质约束 VAD 变化量。
 * traits.playfulness: 放大 arousal 变化（顽皮 → 更容易兴奋）
 * traits.empathy: 放大 valence 变化（共情 → 更容易被用户情绪感染）
 *
 * @param delta 原始的 VAD 变化量
 * @param traits 当前基础人格特质
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
    // 支配度不受基础特质影响
    dominance: delta.dominance,
  };
}

/**
 * 大五人格调制函数。
 * 架构不变量 A10: 使用 BigFiveTraits 的 5 因子对 VAD delta 进行精细调制。
 *
 * 调制规则:
 *   - extraversion: 外向性高 → valence 正方向偏移增强 (gain = 0.5 + extraversion)
 *   - neuroticism: 神经质高 → 所有维度波动放大 (gain = 1 + neuroticism)
 *   - agreeableness: 宜人性高 → 负面 delta 衰减 (negative * (1 - agreeableness * 0.5))
 *   - openness: 开放性高 → arousal 变化增强 (gain = 0.5 + openness)
 *   - conscientiousness: 尽责性高 → dominance 变化抑制 (gain = 1 - conscientiousness * 0.3)
 *
 * @param delta 原始 VAD 变化量（已由 constrainDelta 处理过）
 * @param bigFive 大五人格特质（可选字段，缺失时使用默认值 0.5）
 * @returns 经大五人格调制后的 VAD 变化量
 */
export function modulateDelta(
  delta: AffectiveState,
  bigFive: BigFiveTraits = {}
): AffectiveState {
  const bf = { ...DEFAULT_BIG_FIVE, ...bigFive };

  let { valence, arousal, dominance } = delta;

  // 1. 外向性: valence 正方向增益
  const vGain = 0.5 + bf.extraversion; // 0.5~1.5
  valence *= vGain;

  // 2. 神经质: 所有维度波动放大
  const nGain = 1 + bf.neuroticism; // 1.0~1.3
  valence *= nGain;
  arousal *= nGain;
  dominance *= nGain;

  // 3. 宜人性: 负面 delta 衰减
  if (valence < 0) valence *= 1 - bf.agreeableness * 0.5;
  if (arousal < 0) arousal *= 1 - bf.agreeableness * 0.3;
  if (dominance < 0) dominance *= 1 - bf.agreeableness * 0.3;

  // 4. 开放性: arousal 变化增强
  const aGain = 0.5 + bf.openness; // 0.5~1.5
  arousal *= aGain;

  // 5. 尽责性: dominance 变化抑制
  const dSuppress = 1 - bf.conscientiousness * 0.3; // 0.7~1.0
  dominance *= dSuppress;

  return { valence, arousal, dominance };
}

/**
 * 完整调制管线：先基础特质约束，再大五人格调制。
 * 提供给 PersonalityEngine 使用，替代直接调用 constrainDelta。
 *
 * @param delta 原始 VAD 变化量
 * @param traits 基础人格特质
 * @param bigFive 大五人格特质（可选）
 * @returns 经完整调制的 VAD 变化量
 */
export function modulateFull(
  delta: AffectiveState,
  traits: PersonalityTraits,
  bigFive: BigFiveTraits = {}
): AffectiveState {
  const constrained = constrainDelta(delta, traits);
  return modulateDelta(constrained, bigFive);
}
