/**
 * 人格引擎 (PersonalityEngine)
 *
 * 架构不变量 A1: 人格引擎是情感系统的唯一真理源。
 *   所有情感数值的变更必须经过 PersonalityEngine 的 trait 约束。
 *   EmotionInferrer 的输出必须经 applyEvent() 过滤后，才能分发给 Live2D/TTS/Expresser。
 *
 * 架构不变量 A2: 情感衰减不可跳过。
 *   构造时启动 setInterval(decayInterval)，destroy() 时 clearInterval。
 *
 * 职责:
 *   - 维护 VAD 情感状态 (AffectiveState)
 *   - 通过人格特质 (PersonalityTraits) 约束情感变化
 *   - 运行情感衰减 (EmotionDecay)，使 VAD 自然回归基线
 *   - 提供情感事件接口 (applyEvent)，供 EmotionInferrer 和 Perceptor 调用
 */

import { AffectiveState, BASELINE_STATE, emotionToVAD } from './affective';
import { PersonalityTraits, DEFAULT_TRAITS, constrainDelta } from './traits';
import { EmotionDecay } from './decay';

export type { AffectiveState } from './affective';
export type { PersonalityTraits } from './traits';
export { BASELINE_STATE, DEFAULT_TRAITS } from './affective';

/** 情感变更事件类型 */
export type AffectiveEventType = 'user_sad' | 'user_happy' | 'user_angry' | 'user_anxious' | 'tool_success' | 'tool_failure' | 'llm_response';

/** 情感事件定义 */
export interface AffectiveEvent {
  type: AffectiveEventType;
  /** 关联的情绪名称（来自 EmotionInferrer 或 Perceptor） */
  emotion?: string;
  /** 事件强度 0.0~1.0 */
  intensity: number;
}

/** 人格引擎配置 */
export interface PersonalityConfig {
  playfulness: number;
  empathy: number;
  decayIntervalMs: number;
  decayRate: number;
  /** VAD 平滑因子 (0.05~1.0)，默认 0.3。值越小平滑度越高。架构不变量 A8 */
  smoothingFactor: number;
}

/** 默认配置 */
export const DEFAULT_PERSONALITY_CONFIG: PersonalityConfig = {
  playfulness: 0.5,
  empathy: 0.7,
  decayIntervalMs: 300_000,
  decayRate: 0.1,
  smoothingFactor: 0.3,
};

/** 事件 → VAD 变化量映射 */
const EVENT_VAD_DELTA: Record<AffectiveEventType, AffectiveState> = {
  user_sad: { valence: -0.3, arousal: -0.1, dominance: -0.15 },
  user_happy: { valence: 0.2, arousal: 0.15, dominance: 0.1 },
  user_angry: { valence: -0.25, arousal: 0.3, dominance: 0.2 },
  user_anxious: { valence: -0.15, arousal: 0.2, dominance: -0.2 },
  tool_success: { valence: 0.1, arousal: 0.05, dominance: 0.1 },
  tool_failure: { valence: -0.1, arousal: -0.05, dominance: -0.1 },
  llm_response: { valence: 0.05, arousal: 0.02, dominance: 0.02 },
};

/** 情感变更监听器 */
export type AffectiveListener = (state: AffectiveState, previous: AffectiveState) => void;

/** 人格引擎 */
export class PersonalityEngine {
  /** 当前输出 VAD（经 EMA 平滑后的值），是 getAffective() 的唯一直达出口。架构不变量 A1 */
  private current: AffectiveState;
  /** 目标 VAD（applyEvent() 直接写入的目标值），由 smoothTick() 逐步将 current 拉向 target。架构不变量 A8 */
  private target: AffectiveState;
  /** EMA 平滑因子 */
  private smoothingFactor: number;
  private traits: PersonalityTraits;
  private decay: EmotionDecay;
  private listeners: Set<AffectiveListener> = new Set();
  /** 平滑定时器（每 100ms 执行一次 smoothTick） */
  private smoothTimer: ReturnType<typeof setInterval> | null = null;

  constructor(config: Partial<PersonalityConfig> = {}) {
    const cfg = { ...DEFAULT_PERSONALITY_CONFIG, ...config };
    this.current = { ...BASELINE_STATE };
    this.target = { ...BASELINE_STATE };
    this.smoothingFactor = cfg.smoothingFactor;
    this.traits = {
      playfulness: cfg.playfulness,
      empathy: cfg.empathy,
    };
    this.decay = new EmotionDecay(cfg.decayIntervalMs, cfg.decayRate);

    // 架构不变量 A2: 情感衰减不可跳过 — 启动定时衰减
    this.decay.start((_state) => {
      // 衰减回调：对当前和目标 VAD 执行衰减计算
      const previous = { ...this.current };
      this.current = this.decay.apply(this.current);
      this.target = this.decay.apply(this.target);
      this.notifyListeners(previous);
    });

    // 架构不变量 A8: 启动平滑定时器，每 100ms 将 current 向 target 靠近
    this.smoothTimer = setInterval(() => {
      this.smoothTick();
    }, 100);
  }

  // ==================== 读取接口 ====================

  /** 获取当前 VAD 情感状态 */
  getAffective(): AffectiveState {
    return { ...this.current };
  }

  /** 获取当前人格特质 */
  getTraits(): PersonalityTraits {
    return { ...this.traits };
  }

  // ==================== 事件接口 ====================

  /**
   * 应用情感事件。
   * 架构不变量 A1: 所有情感变化经过 trait 约束。
   * 架构不变量 A8: VAD 变化必须经过平滑滤波 — 原始值写入 target，不直接更新 current。
   *
   * @param event 情感事件
   * @returns 变更后的 VAD 状态（注意: 返回的是当前 current，新事件需经 smoothTick 逐步体现）
   */
  applyEvent(event: AffectiveEvent): AffectiveState {
    // 如果事件携带 emotion，直接映射到 VAD
    if (event.emotion) {
      const emoVAD = emotionToVAD(event.emotion);
      // 向目标 VAD 偏移，偏移量受 intensity 和 trait 约束
      const rawDelta: AffectiveState = {
        valence: (emoVAD.valence - BASELINE_STATE.valence) * event.intensity,
        arousal: (emoVAD.arousal - BASELINE_STATE.arousal) * event.intensity,
        dominance: (emoVAD.dominance - BASELINE_STATE.dominance) * event.intensity,
      };
      const delta = constrainDelta(rawDelta, this.traits);
      this.target = this.applyDeltaToTarget(delta);
    } else {
      // 基于事件类型映射
      const rawDelta = EVENT_VAD_DELTA[event.type];
      if (rawDelta) {
        // 应用强度缩放
        const scaled: AffectiveState = {
          valence: rawDelta.valence * event.intensity,
          arousal: rawDelta.arousal * event.intensity,
          dominance: rawDelta.dominance * event.intensity,
        };
        const delta = constrainDelta(scaled, this.traits);
        this.target = this.applyDeltaToTarget(delta);
      }
    }

    // 架构不变量 A8: 不直接通知监听器，由 smoothTick() 在 current 变化时通知
    return { ...this.current };
  }

  /**
   * 直接设置 VAD 状态（仅供 EmotionInferrer 输出集成使用）。
   * 架构不变量 A1: 设置前自动经过 trait 约束检查。
   * 架构不变量 A8: 同时设置 target 和 current，触发一次平滑通知。
   */
  setAffective(state: AffectiveState): void {
    const previous = { ...this.current };
    const clamped = {
      valence: this.clamp(state.valence),
      arousal: this.clamp(state.arousal),
      dominance: this.clamp(state.dominance),
    };
    this.target = { ...clamped };
    this.current = { ...clamped };
    this.notifyListeners(previous);
  }

  // ==================== 配置更新 ====================

  /** 更新人格特质 */
  updateTraits(traits: Partial<PersonalityTraits>): void {
    this.traits = { ...this.traits, ...traits };
  }

  /** 更新衰减参数 */
  updateDecay(intervalMs: number, rate: number, noiseAmplitude?: number): void {
    this.decay.stop();
    this.decay.updateConfig(intervalMs, rate, noiseAmplitude);
    this.decay.start((_state) => {
      const previous = { ...this.current };
      this.current = this.decay.apply(this.current);
      this.target = this.decay.apply(this.target);
      this.notifyListeners(previous);
    });
  }

  // ==================== 监听器 ====================

  /** 注册情感变更监听器 */
  onAffectiveChange(listener: AffectiveListener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /** 移除所有监听器 */
  clearListeners(): void {
    this.listeners.clear();
  }

  // ==================== 生命周期 ====================

  /** 销毁引擎，停止衰减和平滑定时器 */
  destroy(): void {
    this.decay.stop();
    if (this.smoothTimer) {
      clearInterval(this.smoothTimer);
      this.smoothTimer = null;
    }
    this.listeners.clear();
  }

  // ==================== 内部方法 ====================

  /**
   * EMA 平滑步进：将 current 向 target 靠近 smoothingFactor 比例。
   * 架构不变量 A8: VAD 变化必须经过平滑滤波。
   */
  smoothTick(): void {
    const diff: AffectiveState = {
      valence: (this.target.valence - this.current.valence) * this.smoothingFactor,
      arousal: (this.target.arousal - this.current.arousal) * this.smoothingFactor,
      dominance: (this.target.dominance - this.current.dominance) * this.smoothingFactor,
    };
    // 无显著变化时跳过，避免无效通知
    if (Math.abs(diff.valence) < 0.0001 && Math.abs(diff.arousal) < 0.0001 && Math.abs(diff.dominance) < 0.0001) {
      return;
    }
    const previous = { ...this.current };
    this.current = {
      valence: this.clamp(this.current.valence + diff.valence),
      arousal: this.clamp(this.current.arousal + diff.arousal),
      dominance: this.clamp(this.current.dominance + diff.dominance),
    };
    this.notifyListeners(previous);
  }

  private applyDelta(delta: AffectiveState): AffectiveState {
    return {
      valence: this.clamp(this.current.valence + delta.valence),
      arousal: this.clamp(this.current.arousal + delta.arousal),
      dominance: this.clamp(this.current.dominance + delta.dominance),
    };
  }

  /** 将 delta 应用到 target（架构不变量 A8: 事件写入 target） */
  private applyDeltaToTarget(delta: AffectiveState): AffectiveState {
    return {
      valence: this.clamp(this.target.valence + delta.valence),
      arousal: this.clamp(this.target.arousal + delta.arousal),
      dominance: this.clamp(this.target.dominance + delta.dominance),
    };
  }

  private clamp(value: number): number {
    return Math.max(0, Math.min(1, value));
  }

  private notifyListeners(previous: AffectiveState): void {
    const current = { ...this.current };
    this.listeners.forEach((listener) => {
      try {
        listener(current, previous);
      } catch {
        // 单个监听器异常不影响其他监听器
      }
    });
  }
}
