/**
 * 情绪推断引擎（架构不变量 #8, A1）
 *
 * 架构不变量 #8: 情绪推断结果必须同步输出 emotion + action 参数
 * 架构不变量 A1: 输出必须携带 VAD 三轴值（来源 PersonalityEngine）
 *
 * 解析 LLM 回复中的情感标签，输出 EmotionUpdatePayload（含 VAD）
 */

import { EmotionUpdatePayload } from '../shared/ipc-protocol';
import { emotionToVAD, vadToEmotion } from './personality/affective';
import type { AffectiveState } from './personality/affective';

/** 情绪标签正则 — 匹配 [emotion:xxx] 和 [action:xxx] */
const EMOTION_TAG_REGEX = /\[emotion:\s*([a-zA-Z\u4e00-\u9fff]+)\]/;
const ACTION_TAG_REGEX = /\[action:\s*([a-zA-Z\u4e00-\u9fff]+)\]/;

/** 结构化 JSON 模式的正则 — 匹配顶层 JSON 对象 */
const JSON_BLOCK_REGEX = /\{[\s\S]*?"text"[\s\S]*?"emotion"[\s\S]*?\}/;

/** 默认情绪/动作映射 */
const DEFAULT_EMOTION_ACTION_MAP: Record<string, string> = {
  happy: 'smile',
  高兴: 'smile',
  sad: 'sigh',
  悲伤: 'sigh',
  angry: 'shake',
  愤怒: 'shake',
  surprised: 'cheer',
  惊讶: 'cheer',
  calm: 'idle',
  平静: 'idle',
  confused: 'think',
  困惑: 'think',
  anxious: 'think',
  焦虑: 'think',
  shy: 'blush',
  害羞: 'blush',
  excited: 'cheer',
  兴奋: 'cheer',
  tired: 'yawn',
  疲倦: 'yawn',
};

/** 情绪推断结果 */
export interface EmotionInferenceResult {
  /** 提取出的纯文本（不含标签） */
  text: string;
  /** 情绪更新载荷 */
  emotionUpdate: EmotionUpdatePayload;
}

/** 情绪推断器 */
export class EmotionInferrer {
  private emotionActionMap: Record<string, string>;

  constructor(customMap?: Record<string, string>) {
    this.emotionActionMap = { ...DEFAULT_EMOTION_ACTION_MAP, ...customMap };
  }

  /**
   * 推断情绪，支持两种模式：
   * a. 结构化 JSON 模式: {"text": "...", "emotion": "happy", "action": "wave"}
   * b. 标签内嵌模式: 从回复文本中提取 [emotion:happy] [action:wave] 标签
   *
   * 输出包含 VAD 三轴值（架构不变量 A1）
   */
  infer(llmResponse: string): EmotionInferenceResult {
    // 优先尝试结构化 JSON 模式
    const jsonResult = this.tryParseJsonMode(llmResponse);
    if (jsonResult) {
      return jsonResult;
    }

    // 回退到标签内嵌模式
    return this.parseTagMode(llmResponse);
  }

  /**
   * 从情绪名称构建含 VAD 的 EmotionUpdatePayload
   * 供 Agent Orchestrator 中的 ExpresserAgent 调用
   */
  buildVADPayload(emotion: string, action: string, intensity: number): EmotionUpdatePayload {
    const vad = emotionToVAD(emotion);
    return {
      emotion,
      action,
      intensity: this.clampIntensity(intensity),
      valence: vad.valence,
      arousal: vad.arousal,
      dominance: vad.dominance,
    };
  }

  /**
   * 从 VAD 状态构建 EmotionUpdatePayload
   * 供 PersonalityEngine 调用
   */
  buildPayloadFromVAD(vad: AffectiveState, intensity: number): EmotionUpdatePayload {
    const emotion = vadToEmotion(vad);
    const action = this.mapEmotionToAction(emotion);
    return {
      emotion,
      action,
      intensity: this.clampIntensity(intensity),
      valence: vad.valence,
      arousal: vad.arousal,
      dominance: vad.dominance,
    };
  }

  /**
   * 模式 a: 结构化 JSON 模式
   * 尝试从 LLM 回复中提取外层 JSON 对象，解析 text / emotion / action / intensity / valence / arousal / dominance 字段
   */
  private tryParseJsonMode(response: string): EmotionInferenceResult | null {
    const match = response.match(JSON_BLOCK_REGEX);
    if (!match) {
      return null;
    }

    try {
      const parsed = JSON.parse(match[0]) as {
        text?: string;
        emotion?: string;
        action?: string;
        intensity?: number;
        valence?: number;
        arousal?: number;
        dominance?: number;
      };

      if (!parsed.text) {
        return null;
      }

      const emotion = this.normalizeEmotion(parsed.emotion || 'calm');
      const action = parsed.action || this.mapEmotionToAction(emotion);
      const intensity = this.clampIntensity(parsed.intensity ?? 0.5);

      // 构建 VAD 值 — 优先使用 JSON 中显式指定的，否则从 emotion 映射
      const vad = emotionToVAD(emotion);
      const payload: EmotionUpdatePayload = {
        emotion,
        action,
        intensity,
        valence: parsed.valence ?? vad.valence,
        arousal: parsed.arousal ?? vad.arousal,
        dominance: parsed.dominance ?? vad.dominance,
      };

      return {
        text: parsed.text,
        emotionUpdate: payload,
      };
    } catch {
      return null;
    }
  }

  /**
   * 模式 b: 标签内嵌模式
   * 从回复文本中提取 [emotion:xxx] [action:xxx] 标签
   */
  private parseTagMode(response: string): EmotionInferenceResult {
    let text = response;
    let emotion = 'calm';
    let action = 'idle';
    let intensity = 0.5;

    // 提取 [emotion:xxx]
    const emotionMatch = response.match(EMOTION_TAG_REGEX);
    if (emotionMatch) {
      emotion = this.normalizeEmotion(emotionMatch[1]);
      text = text.replace(emotionMatch[0], '').trim();
    }

    // 提取 [action:xxx]
    const actionMatch = response.match(ACTION_TAG_REGEX);
    if (actionMatch) {
      action = actionMatch[1];
      text = text.replace(actionMatch[0], '').trim();
    }

    // 如果没有指定 action，根据 emotion 推断
    if (!actionMatch) {
      action = this.mapEmotionToAction(emotion);
    }

    // 构建含 VAD 的 payload（架构不变量 A1）
    const vad = emotionToVAD(emotion);

    return {
      text,
      emotionUpdate: {
        emotion,
        action,
        intensity,
        valence: vad.valence,
        arousal: vad.arousal,
        dominance: vad.dominance,
      },
    };
  }

  /** 标准化情绪名称（转小写） */
  private normalizeEmotion(emotion: string): string {
    return emotion.toLowerCase().trim();
  }

  /** 根据情绪映射默认动作 */
  private mapEmotionToAction(emotion: string): string {
    return this.emotionActionMap[emotion] || 'idle';
  }

  /** 限制强度值在 0.0 ~ 1.0 范围内 */
  private clampIntensity(value: number): number {
    return Math.max(0, Math.min(1, value));
  }

  /** 更新情绪-动作映射表 */
  updateEmotionActionMap(map: Record<string, string>): void {
    this.emotionActionMap = { ...this.emotionActionMap, ...map };
  }

  /** 重置为默认映射 */
  resetToDefaultMap(): void {
    this.emotionActionMap = { ...DEFAULT_EMOTION_ACTION_MAP };
  }
}
