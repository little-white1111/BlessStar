/**
 * Expresser（表达器）— Agent 管线出口
 *
 * 架构不变量 A5: 管线最后一步，组装最终响应
 * 架构不变量 A6: 输出层 VAD 与引擎同步
 * 架构不变量 #8: 情绪+动作同步输出
 *
 * 职责：
 *   1. 接收 EmotionInferrer 输出
 *   2. 经 PersonalityEngine 过滤 VAD
 *   3. 读取 SituationalSelector 结果调制输出风格
 *   4. 组装最终 EMOTION_UPDATE 载荷
 *   5. 口头禅文本注入 + 语气调制
 */

import type { EmotionUpdatePayload } from '../../shared/ipc-protocol';
import type { EmotionInferenceResult } from '../emotion-inferrer';
import type { AffectiveState } from '../personality/affective';
import type { CatchphraseMatch } from '../catchphrase/types';
import type { SituationalModulation } from '../persona/types';

/** Expresser 结果 */
export interface ExpressionResult {
  /** 纯文本回复 */
  text: string;
  /** 经 VAD 过滤的情绪更新载荷 */
  emotionUpdate: EmotionUpdatePayload;
  /** 可选的注入口头禅 */
  catchphrase?: CatchphraseMatch;
}

/** 叙事记录回调（B1: Agent 输出写入 L1 观察层） */
export type NarrativeRecordHook = (result: ExpressionResult) => void;

/** Expresser */
export class Expresser {
  /** 叙事记录回调（可选），注册后每次 express 会自动调用（B1） */
  private narrativeHook: NarrativeRecordHook | null = null;

  /**
   * 注册叙事记录回调
   */
  setNarrativeHook(hook: NarrativeRecordHook): void {
    this.narrativeHook = hook;
  }

  /**
   * 表达最终结果
   * @param inference EmotionInferrer 输出
   * @param filteredVAD 经 PersonalityEngine 过滤后的 VAD
   * @param catchphrase 可选的口头禅匹配结果
   * @param modulation 可选的情境人格调制参数（来自 SituationalSelector）
   */
  express(
    inference: EmotionInferenceResult,
    filteredVAD: AffectiveState,
    catchphrase?: CatchphraseMatch,
    modulation?: SituationalModulation,
  ): ExpressionResult {
    const text = catchphrase
      ? this.injectCatchphrase(inference.text, catchphrase)
      : inference.text;

    // 根据情境调制调整强度
    const baseIntensity = inference.emotionUpdate.intensity ?? 0.5;
    const modulatedIntensity = modulation
      ? this.modulateIntensityBySituational(baseIntensity, modulation)
      : baseIntensity;
    const finalIntensity = catchphrase
      ? this.modulateIntensityByCatchphrase(modulatedIntensity, catchphrase)
      : modulatedIntensity;

    const result: ExpressionResult = {
      text,
      emotionUpdate: {
        emotion: inference.emotionUpdate.emotion,
        action: inference.emotionUpdate.action,
        intensity: finalIntensity,
        valence: filteredVAD.valence,
        arousal: filteredVAD.arousal,
        dominance: filteredVAD.dominance,
        mixedEmotions: inference.emotionUpdate.mixedEmotions,
      },
      catchphrase,
    };

    // 触发叙事记录（B1），B7: 叙事子系统故障不影响主管线
    try {
      this.narrativeHook?.(result);
    } catch {
      // B7: 静默吞掉错误，不影响主管线
    }

    return result;
  }

  /**
   * 将口头禅注入到回复文本中
   * 如果回复已包含类似内容，则不重复注入。
   */
  injectCatchphrase(text: string, catchphrase: CatchphraseMatch): string {
    // 如果回复已包含该口头禅，不重复注入
    if (text.includes(catchphrase.text)) {
      return text;
    }

    // 在回复前加上口头禅
    return `${catchphrase.text} ${text}`;
  }

  /**
   * 根据口头禅强度调制情绪强度
   */
  private modulateIntensityByCatchphrase(baseIntensity: number, catchphrase: CatchphraseMatch): number {
    const intensityMap = { low: 0.3, medium: 0.5, high: 0.8 };
    const catchphraseBoost = intensityMap[catchphrase.intensity];
    return Math.min((baseIntensity + catchphraseBoost) / 2, 1.0);
  }

  /**
   * 根据情境调制参数调整强度值。
   * expressiveness 越高，强度放大越多。
   */
  private modulateIntensityBySituational(baseIntensity: number, modulation: SituationalModulation): number {
    // expressiveness 0~1 映射到 0.8~1.2 的缩放因子
    const expressivenessFactor = 0.8 + modulation.expressiveness * 0.4;
    return Math.max(0, Math.min(1, baseIntensity * expressivenessFactor));
  }

  /**
   * 构建 VAD 同步心跳（架构不变量 A6 — 每秒同步）
   */
  buildHeartbeat(vad: AffectiveState, mixedEmotions?: Array<{ emotion: string; probability: number }>): EmotionUpdatePayload {
    return {
      emotion: 'neutral',
      action: 'idle',
      intensity: 0.5,
      valence: vad.valence,
      arousal: vad.arousal,
      dominance: vad.dominance,
      mixedEmotions,
    };
  }
}
