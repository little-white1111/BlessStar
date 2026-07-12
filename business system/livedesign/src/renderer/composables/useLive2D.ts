import { ref, type Ref } from 'vue';

/** Live2D 渲染状态 */
export interface Live2DState {
  emotion: string;
  action: string;
  intensity: number;
}

/** VAD 参数映射接口（架构不变量 A6） */
export interface VADParamMap {
  ParamSmile: number;
  ParamEyeOpen: number;
  ParamAngleX: number;
}

export function useLive2D() {
  const canvasRef: Ref<HTMLCanvasElement | null> = ref(null);
  const vadParams = ref<VADParamMap>({
    ParamSmile: 0.5,
    ParamEyeOpen: 0.5,
    ParamAngleX: 0,
  });
  const state = ref<Live2DState>({
    emotion: 'neutral',
    action: 'idle',
    intensity: 0.5,
  });

  /**
   * 应用情绪/动作参数驱动 Live2D 动画
   * 架构不变量 #8：情绪推断结果驱动 Live2D 表情和动作
   *
   * 实际渲染由 Cubism SDK 接管，此处为集成点。
   * 集成方式：将 canvasRef 传入 Cubism SDK 的渲染器实例，
   * 然后调用 model.setParameterValue(emotion, intensity) 等 API。
   */
  function applyEmotion(emotion: string, action: string, intensity: number): void {
    state.value = { emotion, action, intensity };

    if (!canvasRef.value) return;

    // TODO: 在此处调用 Cubism SDK 的 API 来驱动模型动画
    // 示例: Live2DModel.fromModelJSON(canvasRef, modelPath);
    //        model.setExpression(emotion);
    //        model.startMotion(action);
  }

  /**
   * 应用 VAD 三轴值到 Live2D 混合表情参数（架构不变量 A6）
   * 输出层与引擎 VAD 同步，用于微调表情权重
   */
  function applyVAD(vad: { valence: number; arousal: number; dominance: number }): void {
    vadParams.value = {
      ParamSmile: vad.valence,
      ParamEyeOpen: vad.arousal,
      ParamAngleX: vad.dominance * 2 - 1,
    };

    if (!canvasRef.value) return;

    // TODO: 在此处调用 Cubism SDK 的 API 设置混合参数
    // 示例: model.setParameterValue('ParamSmile', vad.valence);
    //        model.setParameterValue('ParamEyeOpen', vad.arousal);
    //        model.setParameterValue('ParamAngleX', vad.dominance * 2 - 1);
  }

  /** 应用纯动作（不影响情绪） */
  function applyAction(action: string): void {
    state.value = { ...state.value, action };

    if (!canvasRef.value) return;

    // TODO: 在此处调用 Cubism SDK 的 startMotion(action)
  }

  return {
    canvasRef,
    state,
    vadParams,
    applyEmotion,
    applyAction,
    applyVAD,
  };
}
