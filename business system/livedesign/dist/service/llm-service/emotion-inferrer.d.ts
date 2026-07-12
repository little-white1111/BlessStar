/**
 * 情绪推断引擎（架构不变量 #8 — 情绪推断结果必须同步输出 emotion + action 参数）
 * 解析 LLM 回复中的情感标签，输出 EmotionUpdatePayload
 */
import { EmotionUpdatePayload } from '../shared/ipc-protocol';
/** 情绪推断结果 */
export interface EmotionInferenceResult {
    /** 提取出的纯文本（不含标签） */
    text: string;
    /** 情绪更新载荷 */
    emotionUpdate: EmotionUpdatePayload;
}
/** 情绪推断器 */
export declare class EmotionInferrer {
    private emotionActionMap;
    constructor(customMap?: Record<string, string>);
    /**
     * 推断情绪，支持两种模式：
     * a. 结构化 JSON 模式: {"text": "...", "emotion": "happy", "action": "wave"}
     * b. 标签内嵌模式: 从回复文本中提取 [emotion:happy] [action:wave] 标签
     */
    infer(llmResponse: string): EmotionInferenceResult;
    /**
     * 模式 a: 结构化 JSON 模式
     * 尝试从 LLM 回复中提取外层 JSON 对象，解析 text / emotion / action 字段
     */
    private tryParseJsonMode;
    /**
     * 模式 b: 标签内嵌模式
     * 从回复文本中提取 [emotion:xxx] [action:xxx] 标签
     */
    private parseTagMode;
    /** 标准化情绪名称（转小写） */
    private normalizeEmotion;
    /** 根据情绪映射默认动作 */
    private mapEmotionToAction;
    /** 限制强度值在 0.0 ~ 1.0 范围内 */
    private clampIntensity;
    /** 更新情绪-动作映射表 */
    updateEmotionActionMap(map: Record<string, string>): void;
    /** 重置为默认映射 */
    resetToDefaultMap(): void;
}
//# sourceMappingURL=emotion-inferrer.d.ts.map