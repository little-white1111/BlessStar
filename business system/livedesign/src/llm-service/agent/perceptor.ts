/**
 * Perceptor（感知器）— Agent 管线入口
 *
 * 架构不变量 A5: 所有用户请求必须经过 Perceptor → Cognitive → Planner → Executor → Expresser
 * 架构不变量 A7: 输入已归一化为 InputEvent
 * 架构不变量 A11: LLM 感知降级不可丢失能力 — llmPerceive() 不可用时自动降级到 perceive()
 *
 * 职责：
 *   1. 分析用户输入意图
 *   2. 推断用户情感倾向（正则模式匹配或 LLM 语义感知）
 *   3. 生成 AffectiveEvent 供 PersonalityEngine 消费
 */

import type { InputEvent } from '../../shared/ipc-protocol';
import type { AffectiveEventType } from '../personality/engine';
import type { AffectiveState } from '../personality/affective';

/** Perceptor 分析结果 */
export interface PerceptionResult {
  /** 原始输入事件 */
  input: InputEvent;
  /** 推断的情感事件类型 */
  userEmotion: AffectiveEventType;
  /** 情感强度 0.0~1.0 */
  intensity: number;
  /** 是否包含工具调用需求 */
  requiresTools: boolean;
  /** 关键字匹配摘要 */
  keywords: string[];
}

/**
 * LLM 感知结果（架构不变量 A11）
 * 当 llmPerceive() 成功时，提供比 perceive() 更丰富的情感分析
 */
export interface VADWithConfidence {
  /** VAD 三轴值 */
  vad: AffectiveState;
  /** 置信度 0.0~1.0 */
  confidence: number;
  /** 混合情绪 Top-3（可选，架构不变量 A9） */
  mixedEmotions?: Array<{ emotion: string; probability: number }>;
}

/** 默认强度 */
const DEFAULT_INTENSITY = 0.3;

/** 情感关键词映射 */
const EMOTION_PATTERNS: Array<{ pattern: RegExp; type: AffectiveEventType }> = [
  { pattern: /难过|伤心|哭|sad|cry|miss|心痛|hurt|孤单/, type: 'user_sad' },
  { pattern: /开心|高兴|哈哈|haha|happy|love|喜欢|great|wonderful|棒/, type: 'user_happy' },
  { pattern: /生气|愤怒|气死|angry|mad|furious|烦|讨厌/, type: 'user_angry' },
  { pattern: /担心|焦虑|紧张|anxious|worry|nervous|panic|害怕/, type: 'user_anxious' },
];

/** 工具关键词映射 */
const TOOL_PATTERNS = [
  /搜索|查|search|find|look\s+up/,
  /计算|calc|compute|calculate/,
  /翻译|translate/,
  /天气|weather|temperature/,
];

/** 默认 LLM 感知超时(ms) */
const DEFAULT_LLM_TIMEOUT = 5000;

/** LLM 感知提示词模板 */
const LLM_PERCEPTION_PROMPT = `Analyze the user's emotional state from the following text.
Return a JSON object with:
- emotion: one of (happy, sad, angry, anxious, neutral)
- intensity: a number between 0.0 and 1.0
- valence: a number between 0.0 (negative) and 1.0 (positive)
- arousal: a number between 0.0 (calm) and 1.0 (excited)
- dominance: a number between 0.0 (submissive) and 1.0 (dominant)
Text: `;

/** Perceptor */
export class Perceptor {
  /**
   * 感知并分析输入
   * @param input 归一化输入事件
   */
  perceive(input: InputEvent): PerceptionResult {
    const content = input.payload.toLowerCase();
    const keywords: string[] = [];

    // 推断情感
    let userEmotion: AffectiveEventType = 'llm_response';
    for (const { pattern, type } of EMOTION_PATTERNS) {
      const match = content.match(pattern);
      if (match) {
        userEmotion = type;
        keywords.push(match[0]);
        break;
      }
    }

    // 检测工具需求
    const requiresTools = TOOL_PATTERNS.some((p) => p.test(content));
    if (requiresTools) {
      keywords.push('requires_tools');
    }

    return {
      input,
      userEmotion,
      intensity: DEFAULT_INTENSITY,
      requiresTools,
      keywords,
    };
  }

  /**
   * LLM 语义感知 — 调用 LLM 分析用户文本情感。
   * 架构不变量 A11: LLM 不可用时自动降级到 perceive()。
   *
   * @param input 归一化输入事件
   * @param llmFn 可选的 LLM 调用函数（接收提示词，返回 JSON 字符串）
   * @param timeoutMs 超时时间（默认 5000ms）
   * @returns VADWithConfidence（成功时）或降级到 perceive() 的结果
   */
  static async llmPerceive(
    input: InputEvent,
    llmFn?: (prompt: string) => Promise<string>,
    timeoutMs: number = DEFAULT_LLM_TIMEOUT
  ): Promise<PerceptionResult | VADWithConfidence> {
    // 架构不变量 A11: 无 LLM 函数时直接降级
    if (!llmFn) {
      const perceptor = new Perceptor();
      return perceptor.perceive(input);
    }

    try {
      const result = await Promise.race([
        llmFn(LLM_PERCEPTION_PROMPT + input.payload),
        new Promise<never>((_, reject) =>
          setTimeout(() => reject(new Error('LLM perceive timeout')), timeoutMs)
        ),
      ]);

      const parsed = JSON.parse(result as string);

      // 验证返回结构
      if (parsed && typeof parsed.valence === 'number' && typeof parsed.arousal === 'number' && typeof parsed.dominance === 'number') {
        const vad: AffectiveState = {
          valence: Math.max(0, Math.min(1, parsed.valence)),
          arousal: Math.max(0, Math.min(1, parsed.arousal)),
          dominance: Math.max(0, Math.min(1, parsed.dominance)),
        };

        return {
          vad,
          confidence: typeof parsed.intensity === 'number' ? Math.max(0, Math.min(1, parsed.intensity)) : 0.5,
          mixedEmotions: parsed.emotion
            ? [{ emotion: parsed.emotion, probability: 1.0 }]
            : undefined,
        };
      }

      // 解析结果格式不符合预期，降级
      const perceptor = new Perceptor();
      return perceptor.perceive(input);
    } catch {
      // 架构不变量 A11: LLM 超时/异常 → 自动降级正则匹配
      const perceptor = new Perceptor();
      return perceptor.perceive(input);
    }
  }
}
