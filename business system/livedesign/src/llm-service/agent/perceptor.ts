/**
 * Perceptor（感知器）— Agent 管线入口
 *
 * 架构不变量 A5: 所有用户请求必须经过 Perceptor → Cognitive → Planner → Executor → Expresser
 * 架构不变量 A7: 输入已归一化为 InputEvent
 *
 * 职责：
 *   1. 分析用户输入意图
 *   2. 推断用户情感倾向
 *   3. 生成 AffectiveEvent 供 PersonalityEngine 消费
 */

import type { InputEvent } from '../../shared/ipc-protocol';
import type { AffectiveEventType } from '../personality/engine';

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
}
