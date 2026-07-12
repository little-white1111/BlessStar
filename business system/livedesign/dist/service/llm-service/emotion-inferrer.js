"use strict";
/**
 * 情绪推断引擎（架构不变量 #8 — 情绪推断结果必须同步输出 emotion + action 参数）
 * 解析 LLM 回复中的情感标签，输出 EmotionUpdatePayload
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.EmotionInferrer = void 0;
/** 情绪标签正则 — 匹配 [emotion:xxx] 和 [action:xxx] */
const EMOTION_TAG_REGEX = /\[emotion:\s*([a-zA-Z\u4e00-\u9fff]+)\]/;
const ACTION_TAG_REGEX = /\[action:\s*([a-zA-Z\u4e00-\u9fff]+)\]/;
/** 结构化 JSON 模式的正则 — 匹配顶层 JSON 对象 */
const JSON_BLOCK_REGEX = /\{[\s\S]*?"text"[\s\S]*?"emotion"[\s\S]*?\}/;
/** 默认情绪/动作映射 */
const DEFAULT_EMOTION_ACTION_MAP = {
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
/** 情绪推断器 */
class EmotionInferrer {
    emotionActionMap;
    constructor(customMap) {
        this.emotionActionMap = { ...DEFAULT_EMOTION_ACTION_MAP, ...customMap };
    }
    /**
     * 推断情绪，支持两种模式：
     * a. 结构化 JSON 模式: {"text": "...", "emotion": "happy", "action": "wave"}
     * b. 标签内嵌模式: 从回复文本中提取 [emotion:happy] [action:wave] 标签
     */
    infer(llmResponse) {
        // 优先尝试结构化 JSON 模式
        const jsonResult = this.tryParseJsonMode(llmResponse);
        if (jsonResult) {
            return jsonResult;
        }
        // 回退到标签内嵌模式
        return this.parseTagMode(llmResponse);
    }
    /**
     * 模式 a: 结构化 JSON 模式
     * 尝试从 LLM 回复中提取外层 JSON 对象，解析 text / emotion / action 字段
     */
    tryParseJsonMode(response) {
        const match = response.match(JSON_BLOCK_REGEX);
        if (!match) {
            return null;
        }
        try {
            const parsed = JSON.parse(match[0]);
            if (!parsed.text) {
                return null;
            }
            const emotion = this.normalizeEmotion(parsed.emotion || 'calm');
            const action = parsed.action || this.mapEmotionToAction(emotion);
            const intensity = this.clampIntensity(parsed.intensity ?? 0.5);
            return {
                text: parsed.text,
                emotionUpdate: { emotion, action, intensity },
            };
        }
        catch {
            return null;
        }
    }
    /**
     * 模式 b: 标签内嵌模式
     * 从回复文本中提取 [emotion:xxx] [action:xxx] 标签
     */
    parseTagMode(response) {
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
        return {
            text,
            emotionUpdate: { emotion, action, intensity },
        };
    }
    /** 标准化情绪名称（转小写） */
    normalizeEmotion(emotion) {
        return emotion.toLowerCase().trim();
    }
    /** 根据情绪映射默认动作 */
    mapEmotionToAction(emotion) {
        return this.emotionActionMap[emotion] || 'idle';
    }
    /** 限制强度值在 0.0 ~ 1.0 范围内 */
    clampIntensity(value) {
        return Math.max(0, Math.min(1, value));
    }
    /** 更新情绪-动作映射表 */
    updateEmotionActionMap(map) {
        this.emotionActionMap = { ...this.emotionActionMap, ...map };
    }
    /** 重置为默认映射 */
    resetToDefaultMap() {
        this.emotionActionMap = { ...DEFAULT_EMOTION_ACTION_MAP };
    }
}
exports.EmotionInferrer = EmotionInferrer;
//# sourceMappingURL=emotion-inferrer.js.map