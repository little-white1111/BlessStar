import { ref, onMounted, onUnmounted } from 'vue';
import type { EmotionUpdatePayload } from '../../shared/ipc-protocol';

/** 情绪到 Live2D 表情参数的默认映射 */
const EMOTION_MAP: Record<string, string> = {
  happy: 'ParamSmile',
  sad: 'ParamSad',
  angry: 'ParamAngry',
  surprise: 'ParamSurprise',
  neutral: 'ParamNeutral',
  shy: 'ParamBlush',
  thinking: 'ParamThinking',
};

/** 动作到 Live2D 动作名的默认映射 */
const ACTION_MAP: Record<string, string> = {
  idle: 'motion_idle',
  wave: 'motion_wave',
  nod: 'motion_nod',
  shake: 'motion_shake',
  bow: 'motion_bow',
};

/**
 * 将 VAD 三轴值映射为 Live2D 混合表情权重（架构不变量 A6）
 * valence → smile/blush 权重
 * arousal → eye_open / body_lean 权重
 * dominance → body_angle 权重
 */
const VAD_TO_PARAM = {
  valence: 'ParamSmile',
  arousal: 'ParamEyeOpen',
  dominance: 'ParamAngleX',
} as const;

/** VAD 状态接口 */
export interface VADState {
  valence: number;
  arousal: number;
  dominance: number;
}

export function useEmotion(
  onEmotionApplied?: (emotion: string, action: string, intensity: number, vad?: VADState) => void,
) {
  const currentEmotion = ref<string>('neutral');
  const currentAction = ref<string>('idle');
  const currentIntensity = ref<number>(0.5);
  const currentVAD = ref<VADState>({ valence: 0.5, arousal: 0.5, dominance: 0.5 });

  /** 解析情绪更新载荷，映射为 Live2D 参数 */
  function parseEmotionUpdate(payload: EmotionUpdatePayload): void {
    currentEmotion.value = payload.emotion;
    currentAction.value = payload.action;
    currentIntensity.value = payload.intensity;

    // 架构不变量 A6: 提取 VAD 值用于输出层同步
    if (payload.valence !== undefined && payload.arousal !== undefined && payload.dominance !== undefined) {
      currentVAD.value = {
        valence: payload.valence,
        arousal: payload.arousal,
        dominance: payload.dominance,
      };
    }

    // 回调通知上层组件（由 Live2DCanvas 驱动渲染）
    onEmotionApplied?.(
      payload.emotion,
      payload.action,
      payload.intensity,
      currentVAD.value,
    );
  }

  /** 将情绪名映射为 Live2D 表情参数名 */
  function mapEmotionToParam(emotion: string): string {
    return EMOTION_MAP[emotion] || 'ParamNeutral';
  }

  /** 将动作名映射为 Live2D 动作动画名 */
  function mapActionToMotion(action: string): string {
    return ACTION_MAP[action] || 'motion_idle';
  }

  /** 将强度值 (0~1) 映射为 Live2D 参数值范围 */
  function mapIntensityToParamValue(intensity: number): number {
    // Live2D 参数通常范围是 0~1 或 -1~1，这里映射为 0~1
    return Math.max(0, Math.min(1, intensity));
  }

  /**
   * 将 VAD 状态映射为 Live2D 参数值（架构不变量 A6）
   * 输出层每秒与 PersonalityEngine.getAffective() 同步
   */
  function mapVADToParam(vad: VADState): Record<string, number> {
    return {
      [VAD_TO_PARAM.valence]: vad.valence,          // ParamSmile: 0~1
      [VAD_TO_PARAM.arousal]: vad.arousal,          // ParamEyeOpen: 0~1
      [VAD_TO_PARAM.dominance]: vad.dominance * 2 - 1, // ParamAngleX: -1~1
    };
  }

  /** 窗口 emotion-update 事件处理 */
  function handleEmotionUpdate(event: Event): void {
    const customEvent = event as CustomEvent<EmotionUpdatePayload>;
    if (customEvent.detail) {
      parseEmotionUpdate(customEvent.detail);
    }
  }

  onMounted(() => {
    window.addEventListener('emotion-update', handleEmotionUpdate);
  });

  onUnmounted(() => {
    window.removeEventListener('emotion-update', handleEmotionUpdate);
  });

  return {
    currentEmotion,
    currentAction,
    currentIntensity,
    currentVAD,
    parseEmotionUpdate,
    mapEmotionToParam,
    mapActionToMotion,
    mapIntensityToParamValue,
    mapVADToParam,
  };
}
