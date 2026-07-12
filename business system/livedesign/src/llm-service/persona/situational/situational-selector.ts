/**
 * L3 情境人格选择器（Situational Selector）
 *
 * 架构不变量:
 *   C6 — 情境人格不得持久化 — 输出仅在内存，不写入任何文件或 DB
 *   C7 — 情境人格受 L2 自适应人格约束 — 输出值必须在 L2 对应特质的 ±0.2 范围内
 *
 * 根据当前对话上下文、用户情绪和 L2 自适应人格，实时计算出调制参数。
 */

import type { CorePersona, AdaptivePersona, SituationalModulation } from '../types';

/** 情境上下文 */
export interface SituationalContext {
  /** 用户情绪（从 EmotionalInferrer 获取） */
  userEmotion?: string;
  /** 用户消息文本 */
  userMessage: string;
  /** 对话轮次（从 1 开始） */
  turnNumber: number;
  /** 用户 VAD 情感状态 */
  userValence?: number;
}

/**
 * L3 情境人格选择器。
 * 每次对话前调用 select() 获取调制参数，输出仅在内存（架构不变量 C6）。
 */
export class SituationalSelector {
  /**
   * 根据当前上下文和 L2 自适应人格选择情境调制参数。
   *
   * @param core     L1 核心人格（只读）
   * @param adaptive L2 自适应人格
   * @param context  当前对话上下文
   * @returns 情境调制参数（仅在内存，不持久化）
   */
  select(
    core: CorePersona,
    adaptive: AdaptivePersona,
    context: SituationalContext
  ): SituationalModulation {
    // 1. 基础调制来自 L2 自适应人格
    const baseExpressiveness = adaptive.encouragement;
    const baseFormality = 1.0 - adaptive.patience; // 高耐心 → 低正式度（更随意）

    // 2. 根据上下文调制
    const emotionModulation = this.computeEmotionModulation(context);
    const turnModulation = this.computeTurnModulation(context.turnNumber);

    // 3. 合成最终值
    const rawExpressiveness = baseExpressiveness + emotionModulation.expressiveness + turnModulation.expressiveness;
    const rawFormality = baseFormality + emotionModulation.formality + turnModulation.formality;

    // 4. 架构不变量 C7: 受 L2 自适应人格约束（±0.2）
    const expressiveness = this.clampToRange(rawExpressiveness, adaptive.encouragement, 0.2);
    const formality = this.clampToRange(rawFormality, adaptive.patience, 0.2);

    // 5. 语气由核心人格和用户情绪共同决定
    const tone = this.selectTone(core, context);

    return { tone, expressiveness, formality };
  }

  /**
   * 根据用户情绪计算调制量
   */
  private computeEmotionModulation(context: SituationalContext): { expressiveness: number; formality: number } {
    const emotion = context.userEmotion ?? 'neutral';
    const valence = context.userValence ?? 0.5;

    let expressiveness = 0;
    let formality = 0;

    // 积极情绪 → 更高表达力，更低正式度
    if (valence > 0.6) {
      expressiveness += 0.05;
      formality -= 0.05;
    }
    // 消极情绪 → 更高正式度（更谨慎），更低表达力
    else if (valence < 0.4) {
      expressiveness -= 0.03;
      formality += 0.05;
    }

    // 特定情绪调制
    switch (emotion) {
      case 'happy':
      case 'joy':
        expressiveness += 0.05;
        break;
      case 'sad':
      case 'sorrow':
        expressiveness -= 0.02;
        formality += 0.03;
        break;
      case 'angry':
      case 'rage':
        expressiveness -= 0.05;
        formality += 0.08;
        break;
      case 'anxious':
      case 'fear':
        expressiveness += 0.03; // 焦虑时更需要安抚性表达
        formality -= 0.02;
        break;
      case 'confused':
        expressiveness += 0.02;
        formality += 0.02;
        break;
    }

    return { expressiveness, formality };
  }

  /**
   * 根据对话轮次计算调制量
   */
  private computeTurnModulation(turnNumber: number): { expressiveness: number; formality: number } {
    // 早期对话更谨慎，后期更自然
    if (turnNumber <= 3) {
      return { expressiveness: -0.05, formality: 0.1 };
    }
    if (turnNumber <= 10) {
      return { expressiveness: 0, formality: 0.05 };
    }
    // 长期对话，表达更自如
    return { expressiveness: 0.05, formality: -0.05 };
  }

  /**
   * 将值约束到基线值的 ±range 范围内（架构不变量 C7）
   */
  private clampToRange(value: number, baseline: number, range: number): number {
    const min = baseline - range;
    const max = baseline + range;
    return Math.max(0, Math.min(1, Math.max(min, Math.min(max, value))));
  }

  /**
   * 根据核心人格和上下文选择语气
   */
  private selectTone(core: CorePersona, context: SituationalContext): 'warm' | 'neutral' | 'professional' {
    const emotion = context.userEmotion ?? 'neutral';
    const valence = context.userValence ?? 0.5;

    // 高温暖度且用户情绪消极 → warm 语气（共情安抚）
    if (core.warmth >= 0.7 && valence < 0.4) {
      return 'warm';
    }
    // 高理性度且用户情绪中性/积极 → professional 语气
    if (core.rationality >= 0.8 && valence >= 0.4) {
      return 'professional';
    }
    // 用户愤怒 → professional（保持距离，避免火上浇油）
    if (emotion === 'angry' || emotion === 'rage') {
      return 'professional';
    }
    // 用户悲伤/焦虑 → warm
    if (emotion === 'sad' || emotion === 'sorrow' || emotion === 'anxious' || emotion === 'fear') {
      return 'warm';
    }
    // 默认
    return 'neutral';
  }
}
